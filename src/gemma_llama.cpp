#include "gemma_llama.h"

#include "llama.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

namespace {

std::once_flag g_backend_once;
std::atomic<int> g_active_contexts{0};

void copy_error(const std::string &msg, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return;
    }
    const size_t n = msg.size();
    const size_t to_copy = n < buf_size - 1 ? n : buf_size - 1;
    std::memcpy(buf, msg.data(), to_copy);
    buf[to_copy] = '\0';
}

#if defined(LLAMA_API_VERSION) && LLAMA_API_VERSION >= 11000
void llama_backend_init_wrapper(bool use_numa) {
    llama_backend_init(use_numa);
}
#else
void llama_backend_init_wrapper(bool /*use_numa*/) {
    llama_backend_init();
}
#endif

void backend_init_once(bool use_numa) {
    std::call_once(g_backend_once, [use_numa]() {
        llama_backend_init_wrapper(use_numa);
    });
}

uint32_t resolve_seed(int32_t seed) {
    if (seed >= 0) {
        return static_cast<uint32_t>(seed);
    }
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<uint32_t>(now.time_since_epoch().count() & 0xffffffffu);
}

}  // namespace

struct gemma_llama_context {
    llama_model *model;
    llama_context *ctx;
    gemma_runtime_config runtime;
};

void gemma_default_runtime(gemma_runtime_config *cfg) {
    if (!cfg) {
        return;
    }
    cfg->n_ctx = 4096;
    cfg->n_batch = 512;
    cfg->n_threads = 0;
    cfg->n_gpu_layers = 0;
    cfg->seed = -1;
    cfg->main_gpu = 0;
    cfg->split_mode = 0;
    cfg->use_mmap = 1;
    cfg->use_mlock = 0;
    cfg->numa = 0;
}

void gemma_default_sampling(gemma_sampling_config *cfg) {
    if (!cfg) {
        return;
    }
    cfg->max_new_tokens = 256;
    cfg->temperature = 0.7f;
    cfg->top_p = 0.95f;
    cfg->top_k = 40;
    cfg->repetition_penalty = 1.1f;
    cfg->repetition_last_n = 128;
}

int gemma_llama_init(const char *model_path,
                     const gemma_runtime_config *runtime,
                     gemma_llama_t **out_ctx,
                     char *errbuf,
                     size_t errbuf_size) {
    if (!out_ctx) {
        copy_error("out_ctx is null", errbuf, errbuf_size);
        return -1;
    }
    *out_ctx = nullptr;

    if (!model_path || std::strlen(model_path) == 0) {
        copy_error("model_path is empty", errbuf, errbuf_size);
        return -1;
    }

    gemma_runtime_config rt;
    gemma_default_runtime(&rt);
    if (runtime) {
        rt = *runtime;
    }

    backend_init_once(rt.numa > 0);

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = rt.n_gpu_layers;
    mparams.main_gpu = rt.main_gpu;
    mparams.split_mode = static_cast<llama_split_mode>(rt.split_mode);
    mparams.progress_callback = nullptr;
    mparams.use_mmap = rt.use_mmap != 0;
    mparams.use_mlock = rt.use_mlock != 0;

    llama_model *model = llama_model_load_from_file(model_path, mparams);
    if (!model) {
        copy_error("failed to load GGUF model", errbuf, errbuf_size);
        return -1;
    }

    llama_context_params cparams = llama_context_default_params();
    if (rt.n_ctx > 0) {
        cparams.n_ctx = rt.n_ctx;
    }
    if (rt.n_batch > 0) {
        cparams.n_batch = rt.n_batch;
    }
    if (rt.n_threads > 0) {
        cparams.n_threads = rt.n_threads;
        cparams.n_threads_batch = rt.n_threads;
    }

    llama_context *ctx = llama_init_from_model(model, cparams);
    if (!ctx) {
        llama_model_free(model);
        copy_error("failed to create llama context", errbuf, errbuf_size);
        return -1;
    }

    auto *handle = new (std::nothrow) gemma_llama_context();
    if (!handle) {
        llama_free(ctx);
        llama_model_free(model);
        copy_error("failed to allocate context", errbuf, errbuf_size);
        return -1;
    }
    handle->model = model;
    handle->ctx = ctx;
    handle->runtime = rt;

    g_active_contexts.fetch_add(1, std::memory_order_relaxed);
    *out_ctx = handle;
    return 0;
}

int gemma_llama_generate(gemma_llama_t *ctx,
                         const char *prompt,
                         const gemma_sampling_config *sampling,
                         gemma_token_callback on_token,
                         void *user_data,
                         char *errbuf,
                         size_t errbuf_size) {
    if (!ctx || !prompt) {
        copy_error("ctx or prompt is null", errbuf, errbuf_size);
        return -1;
    }

    gemma_sampling_config smp;
    gemma_default_sampling(&smp);
    if (sampling) {
        smp = *sampling;
    }

    const int32_t max_new = smp.max_new_tokens > 0 ? smp.max_new_tokens : 256;
    const int32_t repeat_last_n = smp.repetition_last_n > 0 ? smp.repetition_last_n : 128;
    const float repeat_penalty = smp.repetition_penalty > 0.0f ? smp.repetition_penalty : 1.0f;

    llama_memory_clear(llama_get_memory(ctx->ctx), true);

    const llama_vocab *vocab = llama_model_get_vocab(ctx->model);
    if (!vocab) {
        copy_error("failed to access vocabulary", errbuf, errbuf_size);
        return -1;
    }

    const size_t prompt_len_bytes = std::strlen(prompt);
    int32_t prompt_len = llama_tokenize(vocab, prompt, static_cast<int32_t>(prompt_len_bytes), nullptr, 0, true, true);
    if (prompt_len < 0) {
        prompt_len = -prompt_len;
    }
    if (prompt_len <= 0) {
        copy_error("failed to tokenize prompt", errbuf, errbuf_size);
        return -1;
    }

    const uint32_t ctx_limit = llama_n_ctx(ctx->ctx);
    if (static_cast<uint32_t>(prompt_len) >= ctx_limit) {
        copy_error("prompt is longer than context window", errbuf, errbuf_size);
        return -1;
    }

    std::vector<llama_token> prompt_tokens(static_cast<size_t>(prompt_len));
    if (llama_tokenize(vocab,
                       prompt,
                       static_cast<int32_t>(prompt_len_bytes),
                       prompt_tokens.data(),
                       prompt_len,
                       true,
                       true) < 0) {
        copy_error("failed to tokenize prompt", errbuf, errbuf_size);
        return -1;
    }

    auto chain_params = llama_sampler_chain_default_params();
    chain_params.no_perf = false;
    llama_sampler *sampler = llama_sampler_chain_init(chain_params);
    if (!sampler) {
        copy_error("failed to initialize sampler", errbuf, errbuf_size);
        return -1;
    }

    if (repeat_last_n > 0 && repeat_penalty > 1.0f) {
        llama_sampler *penalties = llama_sampler_init_penalties(repeat_last_n, repeat_penalty, 0.0f, 0.0f);
        if (!penalties) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise repetition penalty sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, penalties);
    }
    if (smp.top_k > 0) {
        llama_sampler *top_k = llama_sampler_init_top_k(smp.top_k);
        if (!top_k) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise top-k sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, top_k);
    }
    if (smp.top_p > 0.0f && smp.top_p < 1.0f) {
        llama_sampler *top_p = llama_sampler_init_top_p(smp.top_p, 1);
        if (!top_p) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise top-p sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, top_p);
    }
    if (smp.temperature <= 0.0f) {
        llama_sampler *greedy = llama_sampler_init_greedy();
        if (!greedy) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise greedy sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, greedy);
    } else {
        llama_sampler *temp = llama_sampler_init_temp(smp.temperature);
        if (!temp) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise temperature sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, temp);

        const uint32_t sampler_seed = resolve_seed(ctx->runtime.seed);
        llama_sampler *dist = llama_sampler_init_dist(sampler_seed);
        if (!dist) {
            llama_sampler_free(sampler);
            copy_error("failed to initialise distribution sampler", errbuf, errbuf_size);
            return -1;
        }
        llama_sampler_chain_add(sampler, dist);
    }

    llama_sampler_reset(sampler);

    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size()));

    bool aborted = false;
    llama_token new_token = 0;
    bool prompt_history_added = false;

    for (int32_t generated = 0; generated < max_new; ++generated) {
        if (llama_decode(ctx->ctx, batch) != 0) {
            llama_sampler_free(sampler);
            copy_error("llama_decode failed", errbuf, errbuf_size);
            return -1;
        }

        if (!prompt_history_added) {
            for (llama_token token : prompt_tokens) {
                llama_sampler_accept(sampler, token);
            }
            prompt_history_added = true;
        }

        llama_token candidate = llama_sampler_sample(sampler, ctx->ctx, -1);
        if (llama_vocab_is_eog(vocab, candidate)) {
            break;
        }

        char piece[512];
        int piece_len = llama_token_to_piece(vocab, candidate, piece, sizeof(piece), 0, true);
        if (piece_len < 0) {
            llama_sampler_free(sampler);
            copy_error("failed to convert token to text", errbuf, errbuf_size);
            return -1;
        }

        if (on_token && piece_len > 0) {
            std::string token_text(piece, piece_len);
            if (!on_token(token_text.c_str(), user_data)) {
                aborted = true;
                break;
            }
        }

        llama_sampler_accept(sampler, candidate);

        new_token = candidate;
        batch = llama_batch_get_one(&new_token, 1);
    }

    llama_sampler_free(sampler);

    return aborted ? 1 : 0;
}

void gemma_llama_free(gemma_llama_t *ctx) {
    if (!ctx) {
        return;
    }
    llama_free(ctx->ctx);
    llama_model_free(ctx->model);
    delete ctx;
    if (g_active_contexts.fetch_sub(1, std::memory_order_relaxed) == 1) {
        llama_backend_free();
    }
}
