# GenShell

GenShell is a ground-up POSIX-style shell written in C with an optional local LLM sidecar powered by llama.cpp. The primary goal today is correctness and safety in the traditional REPL path; model-assisted workflows remain strictly opt-in and live behind a narrow shim.

## 1. Prerequisites
- Clang or GCC with C17 support (Clang is used in all helper scripts).
- `cmake` if you plan to build the bundled `llama.cpp` extras.
- macOS (arm64, Metal) or Linux (x86_64/arm64) – the shell itself is portable, the LLM path requires platform-specific acceleration flags.
- Optional: Python 3.9+ and `pip` for GGUF conversion utilities, `git` for fetching model weights.

After cloning make sure the llama.cpp submodule is available when you need the LLM pieces:

```bash
git submodule update --init --recursive
```

## 2. Building the Shell (default)
The platform helpers now prioritise the shell executable. By default they compile `bin/genshell`; pass extra targets when you need the LLM demo.

- **macOS / Unix-like:**
  ```bash
  ./build_mac.sh          # builds bin/genshell
  ./build_mac.sh all      # builds shell + bin/gemma_cli (expects llama.cpp static libs)
  ```
- **Linux / Windows (MSYS/WSL):**
  ```bash
  ./build_pc.sh           # builds bin/genshell
  ./build_pc.sh all       # builds shell + bin/gemma_cli (expects llama.cpp static libs)
  ```

The scripts respect `CC`, `CXX`, `SHELL_CFLAGS`, `LLM_CFLAGS`, and `LLM_CXXFLAGS` if you need to override toolchains or warning levels. Object files land in `build/obj/`, and artefacts in `bin/`.

To build the llama.cpp static libraries first, use the `*_complete` helpers which run CMake for you and then invoke the corresponding build script with the `all` target:

```bash
./build_mac_complete.sh   # macOS Metal toolchain + both binaries
./build_pc_complete.sh    # CPU-only toolchain + both binaries
```

## 3. Running GenShell
Launch the shell directly once `bin/genshell` exists:

```bash
./bin/genshell
```

Current capabilities include:
- Strategy-dispatched builtins (`cd`, `exit`, `pwd`, `echo`, `export`, `unset`, `umask`) held in separate translation units with documentation headers.
- External command execution with `PATH` lookup, pipes, simple redirections, and environment/tilde expansion.

Known gaps for this milestone:
- Background jobs and job control are not implemented (pipelines always run in the foreground).
- Only basic redirection operators are supported; descriptors such as `2>&1` and here-documents are TODO.
- Word splitting, command substitution, arithmetic expansion, and shell functions are not yet available.

## 4. Repository Layout
```
include/                   # Public headers shared across subsystems
src/
  kernel/
    shell/                 # REPL, parser, executor, builtins
  ctx/                     # Context collectors and YAML helpers
  infra/                   # Low-level OS plumbing (placeholders)
  llm/                     # llama.cpp shim and demo CLI
bin/                       # Build artefacts (genshell, gemma_cli, ...)
models/                    # GGUF checkpoints (ignored by git)
tests/                     # Manual smoke tests and harness stubs
```

## 5. Optional LLM Sidecar
The llama.cpp-based helper (`bin/gemma_cli`) remains available for experimentation. After building the static libs (see §2) you can run:

```bash
./bin/gemma_cli models/<model>.gguf "List a few automation tasks."
```

Preparing Gemma checkpoints follows the standard llama.cpp workflow:
1. Install Python requirements inside `deps/llama.cpp` (`python3 -m pip install -r requirements.txt`).
2. Download weights with `huggingface_hub` or your preferred tooling.
3. Convert to GGUF via `convert_hf_to_gguf.py` and optionally quantise with `llama-quantize`.

These steps mirror the upstream documentation; adjust paths or quantisation levels to fit your hardware.

## 6. Development Notes
- Keep each builtin in its own `src/kernel/shell/builtins/<name>.c` file with a descriptive comment explaining behaviour and edge cases.
- Functions return `0` on success and negative values for internal errors (`gs_shell.h` lists the common constants). Propagate `errno` semantics where practical.
- Run `clang-format` on touched sources before committing (the binary may not be installed on all hosts—install it when possible and reformat touched files).
- Log meaningful repository changes in `NOTES.md` using the timestamped format described in the contributor guide.

## 7. Tests & Verification
Structured tests are still minimal. Run the smoke tests from the repo root:

```bash
./tests/run_tests.sh
```

Until automated coverage lands, manually exercise the shell on macOS and Linux before merging significant changes.
