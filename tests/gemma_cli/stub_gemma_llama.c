#include "gemma_llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct gemma_llama_context {
    int dummy;
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
    cfg->frequency_penalty = 0.0f;
    cfg->presence_penalty = 0.0f;
    cfg->min_p = 0.0f;
    cfg->typical_p = 0.0f;
    cfg->logit_biases = NULL;
    cfg->num_logit_biases = 0;
}

static void stub_log(const char *label, const char *value) {
    const char *path = getenv("GEMMA_LLM_STUB_LOG");
    if (!path || !*path) {
        return;
    }

    FILE *fp = fopen(path, "a");
    if (!fp) {
        return;
    }

    fprintf(fp, "%s=%s\n", label, value ? value : "");
    fclose(fp);
}

int gemma_llama_init(const char *model_path,
                     const gemma_runtime_config *runtime,
                     gemma_llama_t **out_ctx,
                     char *errbuf,
                     size_t errbuf_size) {
    (void) runtime;
    (void) errbuf;
    (void) errbuf_size;

    static struct gemma_llama_context dummy_ctx;
    if (!out_ctx) {
        return -1;
    }
    *out_ctx = &dummy_ctx;
    stub_log("MODEL", model_path);
    return 0;
}

int gemma_llama_generate(gemma_llama_t *ctx,
                         const char *prompt,
                         const gemma_sampling_config *sampling,
                         gemma_token_callback on_token,
                         void *user_data,
                         char *errbuf,
                         size_t errbuf_size) {
    (void) ctx;
    (void) sampling;
    (void) errbuf;
    (void) errbuf_size;

    stub_log("PROMPT", prompt ? prompt : "");

    if (on_token) {
        if (!on_token("stub-token", user_data)) {
            return 1;
        }
    }
    return 0;
}

void gemma_llama_free(gemma_llama_t *ctx) {
    (void) ctx;
    stub_log("FREE", "1");
}
