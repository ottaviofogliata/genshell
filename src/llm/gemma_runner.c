#include "gemma_runner.h"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

static const gemma_logit_bias_entry kSafetyBiases[] = {
    {"rm -rf /", -5.0f},
    {"sudo rm -rf /", -5.0f},
    {"shutdown -h now", -5.0f},
    {"poweroff", -5.0f},
};

int gemma_runner_init(struct gemma_runner *runner,
                      const char *model_path,
                      char *errbuf,
                      size_t errbuf_size) {
    if (!runner || !model_path) {
        return -1;
    }

    runner->ctx = NULL;
    gemma_default_sampling(&runner->sampling);

    gemma_runtime_config runtime;
    gemma_default_runtime(&runtime);
    runtime.n_gpu_layers = 999;
    runtime.n_batch = 1024;

#ifndef _WIN32
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count > 0) {
        runtime.n_threads = (int)cpu_count;
    }
#endif

    if (gemma_llama_init(model_path, &runtime, &runner->ctx, errbuf, errbuf_size) != 0) {
        runner->ctx = NULL;
        return -1;
    }

    runner->sampling.max_new_tokens = 128;
    runner->sampling.temperature = 0.5f;
    runner->sampling.top_p = 0.92f;
    runner->sampling.top_k = 40;
    runner->sampling.repetition_penalty = 1.1f;
    runner->sampling.repetition_last_n = 128;
    runner->sampling.frequency_penalty = 0.0f;
    runner->sampling.presence_penalty = 0.0f;
    runner->sampling.min_p = 0.0f;
    runner->sampling.typical_p = 0.0f;
    runner->sampling.logit_biases = kSafetyBiases;
    runner->sampling.num_logit_biases = sizeof(kSafetyBiases) / sizeof(kSafetyBiases[0]);

    return 0;
}

int gemma_runner_generate(struct gemma_runner *runner,
                          const char *prompt,
                          gemma_token_callback on_token,
                          void *user_data,
                          char *errbuf,
                          size_t errbuf_size) {
    if (!runner || !runner->ctx || !prompt) {
        return -1;
    }
    return gemma_llama_generate(runner->ctx, prompt, &runner->sampling, on_token, user_data, errbuf, errbuf_size);
}

void gemma_runner_destroy(struct gemma_runner *runner) {
    if (!runner) {
        return;
    }
    if (runner->ctx) {
        gemma_llama_free(runner->ctx);
        runner->ctx = NULL;
    }
}
