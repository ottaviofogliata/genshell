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

## 2. Build llama.cpp Tools
1. From the GenShell repo root, step into the bundled llama.cpp sources and build the core library plus helper binaries:
   ```bash
   cd deps/llama.cpp
   cmake -S . -B build -DLLAMA_METAL=on    # drop -DLLAMA_METAL if you are on Linux/Windows
   cmake --build build --target llama llama-quantize
   ```
   This produces `build/libllama.a`, the `quantize` tool, and all required headers under the submodule.

## 3. Prepare a Gemma 3 GGUF Checkpoint
Stay inside `deps/llama.cpp` (or return to it before running these commands).

1. Download the Gemma weights (example uses Google's 4B instruction-tuned release):
   ```bash
   python3 -m pip install -r requirements.txt
   hf download google/gemma-3-4b-it --include "*" \
       --local-dir ../../models/gemma-3-4b-it \
       --local-dir-use-symlinks False
   # The weights now live in genshell/models/gemma-3-4b-it
   ```
2. Convert to a float16 GGUF base file:
   ```bash
   python3 ./convert_hf_to_gguf.py \
       ../../models/gemma-3-4b-it \
       --outfile ../../models/gemma-3-4b-it-f16.gguf \
       --outtype f16 \
       --model-name gemma
   ```
   - `--model-name gemma` nudges the converter toward the correct architecture.
   - `--outtype f16` preserves fidelity before any quantization step.
3. (Optional) Quantize the exported model with the tool you just built:
   ```bash
   ./build/bin/llama-quantize \
       ../../models/gemma-3-4b-it-f16.gguf \
       ../../models/gemma-3-4b-it-q4_K_M.gguf \
       q4_K_M
   ```
   - Choose a quantization scheme that matches your memory/perf budget:
     - `q8_0`, `q6_k`, `q5_0`, `q5_1` – high fidelity 8/6/5-bit formats.
     - `q4_0`, `q4_1`, `q4_K_M`, `q4_K_S`, `q4_K_L` – balanced 4-bit options (ideal on Apple Silicon).
     - `q3_K_M`, `q3_K_L`, `q3_K_S` – aggressive 3-bit variants when memory is tight.
     - `q2_K`, `q2_K_S`, `q2_K_L` – ultra-compact 2-bit variants for experimentation.
   - Quantization naming notes:
     - `q4_0`/`q4_1` are legacy layouts; `q4_1` keeps a bias term for better quality.
     - `q4_K_*` formats use the newer K-block layout; `K_M` (medium) is the usual sweet spot, `K_S` favors size, `K_L` maximizes fidelity.
     - `_K` variants tend to outperform `_0/_1` on Metal thanks to better vectorization.
   - For Gemma 4B on M-series Macs, `f16` or `q4_K_M` typically balance quality and speed.

## 4. Build the Gemma CLI
From the GenShell repo root (run `cd ../..` if you are still inside `deps/llama.cpp`):
```bash
mkdir -p bin
clang++ -std=c++20 \
    -Iinclude -I"deps/llama.cpp" \
    src/gemma_llama.cpp src/gemma_cli.c \
    -L"deps/llama.cpp/build" -lllama \
    -framework Accelerate -framework Metal -framework MetalKit \
    -o bin/gemma_cli
```
- Drop the `-framework` flags on Linux; add `-fopenmp` or `-pthread` if your llama.cpp build requires them.
- If you built a shared library instead, replace `-L/-lllama` with the shared object path.

### Using CMake (optional)
If you prefer CMake, add llama.cpp as an external project or set `LLAMA_ROOT` and create a simple `CMakeLists.txt` that links against `llama`. The manual compile above is sufficient for prototyping.

## 5. Run the Model
```bash
./bin/gemma_cli models/gemma-3-4b-it-q4_K_M.gguf "List three creative shell automation ideas."
```
- First argument: path to your GGUF file. Defaults to `models/gemma-3-text-4b-it-4bit.gguf` if omitted; point it to the f16 file if you skipped quantization.
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
