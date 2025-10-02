#ifndef GEMMA_LLAMA_H
#define GEMMA_LLAMA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque handle that owns llama.cpp model and context state.
 */
typedef struct gemma_llama_context gemma_llama_t;

/**
 * Streaming callback invoked for each token piece produced by the generator.
 * Returning 0 aborts generation early, any non-zero value continues sampling.
 */
typedef int (*gemma_token_callback)(const char *token_text, void *user_data);

/**
 * Runtime parameters that control context creation. Leave fields zero'd to use
 * defaults tuned for Gemma 3 4B on Apple Silicon.
 */
typedef struct {
    int32_t n_ctx;        /* Context window (tokens) */
    int32_t n_batch;      /* Batch size passed to llama_decode */
    int32_t n_threads;    /* CPU threads; <=0 lets llama.cpp pick */
    int32_t n_gpu_layers; /* Number of layers to offload to GPU; 0 = CPU only */
    int32_t seed;         /* RNG seed for sampling; <0 uses time-based */
    int32_t main_gpu;     /* GPU device index when using Metal/CUDA */
    int32_t split_mode;   /* Balance across GPUs; mirror llama_split_mode */
    int32_t use_mmap;     /* Treat as boolean flag */
    int32_t use_mlock;    /* Treat as boolean flag */
    int32_t numa;         /* enable NUMA optimisations when >0 */
} gemma_runtime_config;

/**
 * Sampling behaviour tweaks for text generation.
 */
typedef struct {
    int32_t max_new_tokens;    /* <=0 defaults to 256 */
    float temperature;         /* <=0 locks to greedy */
    float top_p;               /* 0 disables, default 0.95 */
    int32_t top_k;             /* <=0 disables, default 40 */
    float repetition_penalty;  /* >=1, default 1.1 */
    int32_t repetition_last_n; /* <=0 defaults to 128 */
} gemma_sampling_config;

/**
 * Writes recommended defaults into the runtime config struct.
 */
void gemma_default_runtime(gemma_runtime_config *cfg);

/**
 * Writes recommended defaults into the sampling config struct.
 */
void gemma_default_sampling(gemma_sampling_config *cfg);

/**
 * Loads a Gemma 3 4B instruction-tuned model in GGUF format using llama.cpp.
 *
 * @param model_path Path to the GGUF weights (convert via convert-hf-to-gguf.py).
 * @param runtime Optional runtime config; pass NULL for defaults.
 * @param out_ctx Receives the initialized context on success.
 * @param errbuf Optional user buffer to inspect errors.
 * @param errbuf_size Size of errbuf in bytes.
 * @return 0 on success, negative value on failure.
 */
int gemma_llama_init(const char *model_path,
                     const gemma_runtime_config *runtime,
                     gemma_llama_t **out_ctx,
                     char *errbuf,
                     size_t errbuf_size);

/**
 * Generates text streamed through the provided callback.
 *
 * @param ctx Context returned by gemma_llama_init.
 * @param prompt UTF-8 prompt string.
 * @param sampling Optional sampling overrides; NULL applies defaults.
 * @param on_token Optional streaming callback (may be NULL for buffered use).
 * @param user_data User pointer forwarded to callback.
 * @param errbuf Optional buffer for errors.
 * @param errbuf_size Size of errbuf.
 * @return 0 when full generation succeeds, >0 if aborted by callback, <0 on error.
 */
int gemma_llama_generate(gemma_llama_t *ctx,
                         const char *prompt,
                         const gemma_sampling_config *sampling,
                         gemma_token_callback on_token,
                         void *user_data,
                         char *errbuf,
                         size_t errbuf_size);

/**
 * Releases llama.cpp resources. Safe to call on NULL handles.
 */
void gemma_llama_free(gemma_llama_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* GEMMA_LLAMA_H */
