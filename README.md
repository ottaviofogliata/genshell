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
1. From the GenShell repo root, step into the bundled llama.cpp sources and build the core library plus helper binaries.
   - **macOS (Metal):**
     ```bash
     cd deps/llama.cpp
     cmake -S . -B build -DGGML_METAL=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
     cmake --build build --target llama llama-quantize
     ```
   - **Linux / Windows (CPU-only):**
     ```bash
     cd deps/llama.cpp
     cmake -S . -B build -DGGML_METAL=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
     cmake --build build --target llama llama-quantize
     ```
   Building with `BUILD_SHARED_LIBS=OFF` produces the static library `build/src/libllama.a`, the `llama-quantize` tool, and all required headers so you can ship a single self-contained binary.

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

# macOS + Metal (static): compila separatamente C e C++, poi linka
clang  -std=c17  \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_cli.c -o build/obj/gemma_cli.o
clang++ -std=c++20 \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_llama.cpp -o build/obj/gemma_llama.o
clang++ -std=c++20 \
    build/obj/gemma_llama.o build/obj/gemma_cli.o \
    deps/llama.cpp/build/src/libllama.a \
    deps/llama.cpp/build/ggml/src/libggml.a \
    deps/llama.cpp/build/ggml/src/libggml-base.a \
    deps/llama.cpp/build/ggml/src/libggml-cpu.a \
    deps/llama.cpp/build/ggml/src/ggml-blas/libggml-blas.a \
    deps/llama.cpp/build/ggml/src/ggml-metal/libggml-metal.a \
    -framework Accelerate -framework Metal -framework MetalKit \
    -framework Foundation -framework QuartzCore -lobjc \
    -o bin/gemma_cli

# Linux / Windows (CPU-only): compila separatamente C e C++, poi linka
clang  -std=c17  \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_cli.c -o build/obj/gemma_cli.o
clang++ -std=c++20 \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_llama.cpp -o build/obj/gemma_llama.o
clang++ -std=c++20 \
    build/obj/gemma_llama.o build/obj/gemma_cli.o \
    deps/llama.cpp/build/src/libllama.a \
    deps/llama.cpp/build/ggml/src/libggml.a \
    deps/llama.cpp/build/ggml/src/libggml-base.a \
    deps/llama.cpp/build/ggml/src/libggml-cpu.a \
    deps/llama.cpp/build/ggml/src/ggml-blas/libggml-blas.a \
    -lpthread -ldl -o bin/gemma_cli
```
- On macOS the extra Objective‑C runtimes (`-framework Foundation`, `-framework QuartzCore`, `-lobjc`) are required when statically linking the Metal backend.
- On Linux/Windows drop the Apple frameworks and add whatever threading/math libraries your toolchain expects (the example uses `-lpthread -ldl`).
- If you built shared libraries instead, replace the direct `.a` references with the appropriate `-L/-l` pairs.

### Using CMake (optional)
If you prefer CMake, add llama.cpp as an external project or set `LLAMA_ROOT` and create a simple `CMakeLists.txt` that links against `llama`. The manual compile above is sufficient for prototyping.

## 5. Run the Model
```bash
./bin/gemma_cli models/gemma-3-4b-it-q4_K_M "List three creative shell automation ideas."
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
