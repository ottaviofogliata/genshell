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

int32_t resolve_seed(int32_t seed) {
    if (seed >= 0) {
        return seed;
    }
    auto now = std::chrono::high_resolution_clock::now();
    return static_cast<int32_t>(now.time_since_epoch().count() & 0x7fffffff);
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

    llama_model *model = llama_load_model_from_file(model_path, mparams);
    if (!model) {
        copy_error("failed to load GGUF model", errbuf, errbuf_size);
        return -1;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.seed = resolve_seed(rt.seed);
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

    llama_context *ctx = llama_new_context_with_model(model, cparams);
    if (!ctx) {
        llama_free_model(model);
        copy_error("failed to create llama context", errbuf, errbuf_size);
        return -1;
    }

    auto *handle = new (std::nothrow) gemma_llama_context();
    if (!handle) {
        llama_free(ctx);
        llama_free_model(model);
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

    llama_kv_cache_clear(ctx->ctx);

    const int32_t prompt_len = llama_tokenize(ctx->model, prompt, std::strlen(prompt), nullptr, 0, true, false);
    if (prompt_len <= 0) {
        copy_error("failed to tokenize prompt", errbuf, errbuf_size);
        return -1;
    }
    if (prompt_len >= ctx->runtime.n_ctx) {
        copy_error("prompt is longer than context window", errbuf, errbuf_size);
        return -1;
    }

    std::vector<llama_token> input_tokens(prompt_len);
    llama_tokenize(ctx->model, prompt, std::strlen(prompt), input_tokens.data(), prompt_len, true, false);

    const int32_t n_batch = ctx->runtime.n_batch > 0 ? ctx->runtime.n_batch : 512;
    llama_batch batch = llama_batch_init(n_batch, 0, 1);
    int32_t n_past = 0;

    while (n_past < prompt_len) {
        const int32_t n_eval = std::min(n_batch, prompt_len - n_past);
        llama_batch_clear(&batch);
        for (int32_t i = 0; i < n_eval; ++i) {
            batch.token[i] = input_tokens[n_past + i];
            batch.pos[i] = n_past + i;
            batch.seq_id[i][0] = 0;
        }
        batch.n_tokens = n_eval;
        if (llama_decode(ctx->ctx, batch) != 0) {
            llama_batch_free(batch);
            copy_error("llama_decode failed during prompt", errbuf, errbuf_size);
            return -1;
        }
        n_past += n_eval;
    }

    std::vector<llama_token> recent_tokens;
    if (repeat_last_n > 0) {
        size_t take = static_cast<size_t>(std::min<int32_t>(repeat_last_n, prompt_len));
        recent_tokens.insert(recent_tokens.end(),
                             input_tokens.end() - take,
                             input_tokens.end());
    }

    const int32_t vocab = llama_n_vocab(ctx->model);
    std::vector<llama_token_data> candidates;
    candidates.reserve(vocab);

    bool aborted = false;

    for (int32_t iter = 0; iter < max_new; ++iter) {
        const float *logits = llama_get_logits(ctx->ctx);
        if (!logits) {
            copy_error("logits unavailable", errbuf, errbuf_size);
            llama_batch_free(batch);
            return -1;
        }

        candidates.clear();
        for (int32_t token_id = 0; token_id < vocab; ++token_id) {
            candidates.push_back({token_id, logits[token_id], 0.0f});
        }
        llama_token_data_array cand_array{candidates.data(), candidates.size(), false};

        if (repeat_last_n > 0 && repeat_penalty > 1.0f && !recent_tokens.empty()) {
            llama_sample_repetition_penalty(ctx->ctx,
                                            &cand_array,
                                            recent_tokens.data(),
                                            recent_tokens.size(),
                                            repeat_penalty);
        }
        if (smp.top_k > 0) {
            llama_sample_top_k(ctx->ctx, &cand_array, smp.top_k, 1);
        }
        if (smp.top_p > 0.0f && smp.top_p < 1.0f) {
            llama_sample_top_p(ctx->ctx, &cand_array, smp.top_p, 1);
        }

        llama_token selected;
        if (smp.temperature <= 0.0f) {
            selected = llama_sample_token_greedy(ctx->ctx, &cand_array);
        } else {
            llama_sample_temperature(ctx->ctx, &cand_array, smp.temperature);
            selected = llama_sample_token(ctx->ctx, &cand_array);
        }

        if (repeat_last_n > 0) {
            if (recent_tokens.size() >= static_cast<size_t>(repeat_last_n)) {
                recent_tokens.erase(recent_tokens.begin());
            }
            recent_tokens.push_back(selected);
        }

        if (selected == llama_token_eos(ctx->model)) {
            break;
        }

        char piece[512];
        int32_t piece_len = llama_token_to_piece(ctx->model, selected, piece, sizeof(piece));
        std::string token_text;
        if (piece_len > 0) {
            token_text.assign(piece, static_cast<size_t>(piece_len));
        }
        if (on_token) {
            if (!on_token(token_text.c_str(), user_data)) {
                aborted = true;
                break;
            }
        }

        llama_batch_clear(&batch);
        batch.token[0] = selected;
        batch.pos[0] = n_past;
        batch.seq_id[0][0] = 0;
        batch.n_tokens = 1;
        if (llama_decode(ctx->ctx, batch) != 0) {
            llama_batch_free(batch);
            copy_error("llama_decode failed during sampling", errbuf, errbuf_size);
            return -1;
        }
        ++n_past;
    }

    llama_batch_free(batch);

    if (aborted) {
        return 1;
    }
    return 0;
}

void gemma_llama_free(gemma_llama_t *ctx) {
    if (!ctx) {
        return;
    }
    llama_free(ctx->ctx);
    llama_free_model(ctx->model);
    delete ctx;
    if (g_active_contexts.fetch_sub(1, std::memory_order_relaxed) == 1) {
        llama_backend_free();
    }
}
