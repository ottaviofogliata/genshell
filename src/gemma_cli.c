#include "gemma_llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int stdout_token_callback(const char *token, void *user_data) {
    (void)user_data;
    if (!token) {
        return 0;
    }
    fputs(token, stdout);
    fflush(stdout);
    return 1;
}

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <model.gguf> [prompt]\n", prog);
}

int main(int argc, char **argv) {
    const char *model_path = "models/gemma-3-text-4b-it-4bit.gguf";
    const char *prompt = NULL;

    if (argc > 1) {
        model_path = argv[1];
    }
    if (argc > 2) {
        prompt = argv[2];
    }

    if (!prompt) {
        usage(argv[0]);
        fprintf(stderr, "Falling back to default prompt.\n");
        prompt = "Write a haiku about shell automation.";
    }

    gemma_runtime_config runtime;
    gemma_default_runtime(&runtime);
    runtime.n_threads = 0;  /* let llama.cpp auto-detect */

    gemma_llama_t *ctx = NULL;
    char errbuf[512];
    if (gemma_llama_init(model_path, &runtime, &ctx, errbuf, sizeof errbuf) != 0) {
        fprintf(stderr, "Failed to initialize Gemma: %s\n", errbuf);
        return EXIT_FAILURE;
    }

    gemma_sampling_config sampling;
    gemma_default_sampling(&sampling);
    sampling.max_new_tokens = 128;

    fprintf(stdout, "Model: %s\nPrompt: %s\n---\n", model_path, prompt);
    int rc = gemma_llama_generate(ctx, prompt, &sampling, stdout_token_callback, NULL, errbuf, sizeof errbuf);
    if (rc != 0) {
        fprintf(stderr, "\nGeneration error: %s (rc=%d)\n", errbuf, rc);
    } else {
        fputc('\n', stdout);
    }

    gemma_llama_free(ctx);
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
