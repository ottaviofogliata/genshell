# LLM Download & Quantization Guide

GenShell ships with an optional llama.cpp-based sidecar. This guide explains how to fetch and quantize the two supported checkpoints so they can be loaded by the helper binary (currently `bin/gemma_cli`).

> **Heads-up:** The default profile going forward is **Qwen2.5-Coder-3B-Instruct**. Gemma 3 remains available for back-compat testing, but Qwen should be considered the primary target for new work.

## 1. Prerequisites
- Python 3.9+ with `pip`
- `git` (needed for huggingface-cli if you use token-gated repos)
- Optional: [`hf_transfer`](https://github.com/huggingface/hf_transfer) for faster downloads

Install the llama.cpp tooling once per machine:

```bash
python3 -m pip install -r deps/llama.cpp/requirements.txt
```

This installs `huggingface_hub`, `hf_transfer`, and the GGUF conversion utilities referenced below.

Place converted checkpoints under `models/`; the directory is git-ignored.

## 2. Qwen2.5-Coder-3B-Instruct (default)
1. **Download base weights**
   ```bash
   huggingface-cli download Qwen/Qwen2.5-Coder-3B-Instruct --local-dir models/Qwen2.5-Coder-3B-Instruct --local-dir-use-symlinks False
   ```
   Add `--revision main` if you want a specific commit.

2. **Convert to GGUF**
   ```bash
   python3 deps/llama.cpp/convert_hf_to_gguf.py \
       --outtype f16 \
       --outfile models/qwen2.5-coder-3b-instruct-f16.gguf \
       models/Qwen2.5-Coder-3B-Instruct
   ```
   The converter detects the Qwen architecture automatically.

3. **Quantize (recommended Q4_K_M)**
   ```bash
   llama-quantize \
       models/qwen2.5-coder-3b-instruct-f16.gguf \
       models/qwen2.5-coder-3b-instruct-q4_k_m.gguf \
       Q4_K_M
   ```

4. **Smoke test**
   Point the helper CLI at the Qwen checkpoint (for now the binary name is still `gemma_cli`):
   ```bash
   ./bin/gemma_cli models/qwen2.5-coder-3b-instruct-q4_k_m.gguf "Write a cli command only to discover all duplicated songs in the current folder.\n"
   ```

## 3. Gemma (legacy support)
1. **Download base weights**
   ```bash
   huggingface-cli download google/gemma-2-2b-it --local-dir models/gemma-2-2b-it --local-dir-use-symlinks False
   ```
   Substitute another Gemma variant if required.

2. **Convert to GGUF**
   ```bash
   python3 deps/llama.cpp/convert_hf_to_gguf.py \
       --outtype f16 \
       --outfile models/gemma-2-2b-it-f16.gguf \
       models/gemma-2-2b-it
   ```

3. **Quantize (example: Q4_K_M)**
   ```bash
   deps/llama.cpp/quantize \
       models/gemma-2-2b-it-f16.gguf \
       models/gemma-2-2b-it-q4_k_m.gguf \
       Q4_K_M
   ```

4. **Smoke test**
   ```bash
   ./bin/gemma_cli models/gemma-2-2b-it-q4_k_m.gguf "List a few automation tasks."
   ```

## 4. Tips & Troubleshooting
- If you see `No module named sentencepiece`, install it manually: `python3 -m pip install sentencepiece`.
- For machines with limited RAM, quantize straight to `Q4_K_M` or `Q4_0` to avoid holding the f16 model on disk.
- Keep `models/` tidy by removing intermediate files once you are satisfied with the quantized checkpoints.
- LLAMA_CUDA or Metal acceleration flags live inside the build scripts; rebuild `deps/llama.cpp` if you switch toolchains.

Update `NOTES.md` after landing model-related changes so future contributors know which checkpoints were validated.
