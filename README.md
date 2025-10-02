# GenShell LLaMA Runner Quickstart

This guide shows how to prepare a Gemma 3 checkpoint, wire it through the llama.cpp-based shim (`include/gemma_llama.h` / `src/gemma_llama.cpp`), build the sample CLI, and stream generations back into the shell.

## 1. Prerequisites
- Apple Silicon (Metal) or modern x86 CPU with AVX2/AVX512; adjust llama.cpp flags for your platform.
- A recent Clang or GCC with C++20 support.
- CMake (for building llama.cpp).
- Python 3.9+ with `pip` to run the GGUF converter.
- `git` for fetching model weights (optional if you already have them locally).

This repository vendors `llama.cpp` as a submodule under `deps/llama.cpp`. If you just cloned GenShell, remember to pull it in:

```bash
git submodule update --init --recursive
```

Feel free to use a different checkout of llama.cpp; just adjust include/library paths accordingly.

## 2. Prepare a Gemma 3 GGUF Checkpoint
1. Enter the bundled llama.cpp sources (or substitute your own checkout):
   ```bash
   cd deps/llama.cpp
   ```
2. Pull the Gemma weights you care about (examples below use Google's instruction-tuned Gemma 3 4B). The Hugging Face CLI is convenient:
   ```bash
   python3 -m pip install -r requirements.txt --break-system-packages   
   huggingface-cli download google/gemma-3-4b-it --include "*"
   ```
   Alternatively, place the model directory anywhere on disk and reference it during conversion.
3. Convert to GGUF:
   ```bash
   python3 ./convert-hf-to-gguf.py \
       /path/to/google/gemma-3-4b-it \
       --outfile ../../models/gemma-3-4b-it-f16.gguf \
       --outtype f16
   ```
   - Supported `--outtype` values in llama.cpp (pick the smallest that still meets your quality needs):
     - `f32`, `f16`, `bf16` – full or half precision; highest quality, largest files.
     - `q8_0`, `q6_k`, `q5_0`, `q5_1` – high fidelity 8/6/5‑bit quantizations that still need sizable VRAM/RAM.
     - `q4_0`, `q4_1`, `q4_K_M`, `q4_K_S`, `q4_K_L` – balanced 4‑bit modes; good trade-off for Apple Silicon Metal.
     - `q3_K_M`, `q3_K_L`, `q3_K_S` – aggressive 3‑bit options when memory is tight; expect some quality loss.
     - `q2_K`, `q2_K_S`, `q2_K_L` – ultra-compact 2‑bit variants for experimentation or low-resource devices.
   - For Gemma 4B on M-series Macs, `f16` or `q4_K_M` typically strike the right quality/performance balance.
   - Store the resulting `.gguf` under `genshell/models/` (or adjust paths when running the CLI).

## 3. Build llama.cpp as a Library
1. From the llama.cpp directory:
   ```bash
   cmake -S . -B build -DLLAMA_METAL=on         # drop -DLLAMA_METAL if you are on Linux/Windows
   cmake --build build --target llama
   ```
   This produces `build/libllama.a` (static) plus headers under the repo root.
2. Note the absolute paths (adjust if your checkout lives elsewhere):
   - `LLAMA_INC="$(pwd)/deps/llama.cpp"`
   - `LLAMA_LIB="$(pwd)/deps/llama.cpp/build"`

## 4. Build the Gemma CLI
From the GenShell repo root:
```bash
mkdir -p bin
clang++ -std=c++20 \
    -Iinclude -I"$LLAMA_INC" \
    src/gemma_llama.cpp src/gemma_cli.c \
    -L"$LLAMA_LIB" -lllama \
    -framework Accelerate -framework Metal -framework MetalKit \
    -o bin/gemma_cli
```
- Drop the `-framework` flags on Linux; add `-fopenmp` or `-pthread` if your llama.cpp build requires them.
- If you built a shared library instead, replace `-L/-lllama` with the shared object path.

### Using CMake (optional)
If you prefer CMake, add llama.cpp as an external project or set `LLAMA_ROOT` and create a simple `CMakeLists.txt` that links against `llama`. The manual compile above is sufficient for prototyping.

## 5. Run the Model
```bash
./bin/gemma_cli models/gemma-3-4b-it-f16.gguf "List three creative shell automation ideas."
```
- First argument: path to your GGUF file. Defaults to `models/gemma-3-text-4b-it-4bit.gguf` if omitted.
- Second argument: optional prompt string. Without it, the CLI prints a usage hint and falls back to a sample prompt.
- Output streams token-by-token to STDOUT; STDERR surfaces errors (missing model, decode failure, etc.).

## 6. Integrate with GenShell
- `include/gemma_llama.h` describes the minimal C API: initialize once, call `gemma_llama_generate()` with a callback that pushes tokens into the shell's output buffer, and free resources when shutting down the runner.
- The shim is stateless between calls, so you can reuse a single `gemma_llama_t` across prompts. Clear the kv-cache per request (already handled inside `gemma_llama_generate`).
- Adapt the callback to accumulate text until the user accepts it, then emit commands in the REPL.

## 7. Troubleshooting
- **`failed to load GGUF model`** – ensure the path is correct and the llama.cpp build supports the chosen quantization (Metal requires Q4/K or FP16).
- **Linker errors (`undefined symbols for architecture arm64`)** – confirm the `llama.cpp` build target matches your host (arm64 vs x86_64) and that `clang++` sees the correct headers/libs.
- **Slow generation** – bump `runtime.n_threads` before calling `gemma_llama_init`, or convert to a quantized GGUF variant.
- **Context overflow** – the shim defaults to 4,096 tokens. Increase `runtime.n_ctx` (and ensure the model supports it) when initializing.

With the GGUF ready and the CLI compiled, you can script higher-level workflows or embed the shim directly into the GenShell LLM runner module for a fully native experience.
