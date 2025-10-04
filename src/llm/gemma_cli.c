#include "gemma_llama.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

typedef struct {
    size_t tokens_emitted;
    double start_seconds;
} gemma_stream_stats;

static double monotonic_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int stdout_token_callback(const char *token, void *user_data) {
    if (!token) {
        return 0;
    }

    gemma_stream_stats *stats = (gemma_stream_stats *)user_data;
    if (stats) {
        stats->tokens_emitted++;
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
    runtime.n_gpu_layers = 999;  /* 999 means offloads all */
    runtime.n_batch = 1024;      /* larger batch improves GPU throughput */

#ifndef _WIN32
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count > 0) {
        runtime.n_threads = (int)cpu_count;
    }
#endif

    gemma_llama_t *ctx = NULL;
    char errbuf[512];
    if (gemma_llama_init(model_path, &runtime, &ctx, errbuf, sizeof errbuf) != 0) {
        fprintf(stderr, "Failed to initialize Gemma: %s\n", errbuf);
        return EXIT_FAILURE;
    }

    gemma_sampling_config sampling;
    gemma_default_sampling(&sampling);

    /*
     * Configure token-level safety rails. Each entry pairs free-form text with
     * a negative bias so the sampler strongly prefers alternatives. Edit this
     * list to match your deployment requirements (bias magnitudes around -5
     * nearly forbid a token; milder values like -1 merely discourage it).
     */
    static const gemma_logit_bias_entry safety_biases[] = {
        {"rm -rf /", -5.0f},
        {"sudo rm -rf /", -5.0f},
        {"shutdown -h now", -5.0f},
        {"poweroff", -5.0f},
    };
    sampling.logit_biases = safety_biases;
    sampling.num_logit_biases = sizeof(safety_biases) / sizeof(safety_biases[0]);

    /*
     * Sampling controls – tweak these to shape the style of the generated text.
     * The defaults below mirror a “balanced” chat preset. Increase/decrease
     * values gradually (±0.05 or ±5 tokens) to study the impact.
     */

    // Hard limit on how many tokens we ask the model to produce. The effective
    // ceiling is still bounded by the context window (n_ctx - prompt length).
    sampling.max_new_tokens = 128;

    // Temperature controls randomness. 0 = greedy/deterministic, ~0.3 keeps
    // responses factual, ≥1.0 lets the model take creative leaps.
    sampling.temperature = 0.5f;

    // Nucleus sampling (top-p) keeps only the smallest probability mass whose
    // cumulative probability exceeds this value. Lower = focused & safe, higher
    // = more diverse. Typical chat settings live around 0.9–0.95.
    sampling.top_p = 0.92f;

    // Top-k limits the candidate set to the k most likely tokens. Smaller k is
    // safer but can feel repetitive; larger k (or <=0) broadens choices.
    sampling.top_k = 40;

    // Repetition penalty >1 discourages the model from repeating recent tokens.
    // Values in the 1.05–1.2 range are common; 1.0 disables the mechanism.
    sampling.repetition_penalty = 1.1f;

    // Sliding window size (in tokens) that the repetition penalty considers.
    // Use larger windows for long-form writing, smaller for short replies.
    sampling.repetition_last_n = 128;

    // Frequency penalty subtracts weight from tokens that already appeared more
    // often; useful for mitigating “word echo”. Typical values: 0.0–0.5.
    sampling.frequency_penalty = 0.0f;

    // Presence penalty pushes the model to introduce tokens that have not
    // occurred yet. Slight positive values (≈0.1–0.3) boost novelty.
    sampling.presence_penalty = 0.0f;

    // Min-p (a.k.a. tail-free sampling) drops all candidates whose probability
    // falls below this floor. Raises coherence without being as strict as top-p.
    // Set to 0.0 to disable or experiment within 0.05–0.15.
    sampling.min_p = 0.0f;

    // Typical sampling keeps tokens whose information content is close to the
    // expected entropy. Larger values (≈0.8–0.95) still allow variety while
    // steering away from extremely unlikely tokens. Set to 0 to disable.
    sampling.typical_p = 0.0f;

    gemma_stream_stats stats = {
        .tokens_emitted = 0,
        .start_seconds = monotonic_seconds(),
    };

    fprintf(stdout, "Model: %s\nPrompt: %s\n---\n", model_path, prompt);
    int rc = gemma_llama_generate(ctx, prompt, &sampling, stdout_token_callback, &stats, errbuf, sizeof errbuf);
    if (rc != 0) {
        fprintf(stderr, "\nGeneration error: %s (rc=%d)\n", errbuf, rc);
    } else {
        fputc('\n', stdout);
        double elapsed = monotonic_seconds() - stats.start_seconds;
        double tps = elapsed > 0.0 ? stats.tokens_emitted / elapsed : 0.0;
        fprintf(stdout, "Generated %zu tokens in %.2f s (%.2f tok/s)\n",
                stats.tokens_emitted,
                elapsed,
                tps);
    }

    gemma_llama_free(ctx);
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
