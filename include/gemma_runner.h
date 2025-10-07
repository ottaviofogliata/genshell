#ifndef GEMMA_RUNNER_H
#define GEMMA_RUNNER_H

#include "gemma_llama.h"

/*
 * Thin façade around the llama.cpp bindings so the CLI can focus on I/O.  The
 * runner owns the llama context, seeds the default runtime/sampling settings,
 * and exposes a single `generate` API that streams tokens through the provided
 * callback.
 */
struct gemma_runner {
    gemma_llama_t *ctx;
    gemma_sampling_config sampling;
};

int gemma_runner_init(struct gemma_runner *runner,
                      const char *model_path,
                      char *errbuf,
                      size_t errbuf_size);

int gemma_runner_generate(struct gemma_runner *runner,
                          const char *prompt,
                          gemma_token_callback on_token,
                          void *user_data,
                          char *errbuf,
                          size_t errbuf_size);

void gemma_runner_destroy(struct gemma_runner *runner);

#endif /* GEMMA_RUNNER_H */
