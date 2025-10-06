# Build & Platform Guide

GenShell targets macOS (arm64 + Metal) and Linux (x86_64/arm64). The core shell is portable across POSIX-like environments; the llama.cpp helper (Qwen2.5-Coder-3B-Instruct by default) depends on platform-specific acceleration flags. This guide expands on the quick-start instructions.

## Toolchains
- **Compilers:** Clang or GCC with C17 support. Clang is assumed by the helper scripts.
- **Linkers:** Platform defaults (`ld64` on macOS, `ld`/`lld` on Linux). No special configuration is required for the shell target.
- **Optional extras:**
  - `cmake` for building llama.cpp static libraries (`deps/llama.cpp`).
  - Python 3.9+ with `pip` for GGUF conversion scripts.
  - `git` for fetching model weights or submodules.

Set `CC`, `CXX`, or flags such as `SHELL_CFLAGS`, `LLM_CFLAGS`, and `LLM_CXXFLAGS` to override toolchains or warning levels when invoking the build scripts.

## Shell-Only Builds
```bash
./build_mac.sh    # macOS / Unix-like hosts
./build_pc.sh     # Linux or Windows via MSYS/WSL
```

Targets:
- Compiles sources under `src/kernel/shell/` and supporting modules.
- Drops intermediate objects in `build/obj/` and the final binary in `bin/genshell`.

## Shell + LLM Builds
Add the `all` argument to the helpers once llama.cpp libraries are available.
```bash
./build_mac.sh all
./build_pc.sh all
```

These targets depend on static libraries in `deps/llama.cpp/build`. If they are missing, run the `*_complete` scripts first.

### One-Stop LLM Builds
```bash
./build_mac_complete.sh   # macOS Metal toolchain + genshell + gemma_cli
./build_pc_complete.sh    # CPU-only toolchain + genshell + gemma_cli
```

The scripts configure llama.cpp through CMake, build the required static libraries, and then call the main helpers with the `all` target.

## Environment Layout
```
bin/          # genshell, gemma_cli sidecar, additional tools
build/obj/    # Intermediate object files
build/logs/   # Build logs (if enabled by the helpers)
models/       # GGUF weights (ignored by git)
```

## Common Issues
- **Missing clang-format:** The repo expects contributors to format patched files. Install `clang-format` for your platform and re-run on touched files before submitting patches.
- **Uninitialised submodule:** `deps/llama.cpp` must be present before building the LLM helper. Run `git submodule update --init --recursive`.
- **Metal vs. CPU builds:** macOS builds default to Metal. Pass `LLM_CXXFLAGS="-DGGML_USE_CPU"` to force CPU mode if required.

## Running
Once the build succeeds, start the shell directly:
```bash
./bin/genshell
```

Run the demo assistant with a GGUF model:
```bash
./bin/gemma_cli models/qwen2.5-coder-3b-instruct-q4_k_m.gguf "List automation tasks"
```

Full download and quantization steps live in `LLM_DOWNLOAD.md`.

See [LLM Sidecar](llm-sidecar.md) for conversion, quantisation, and safety notes.
