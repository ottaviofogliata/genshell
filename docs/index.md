# GenShell Overview

GenShell is a ground-up POSIX-style shell written in C with an optional local LLM sidecar powered by llama.cpp. The shell focuses on correctness, scripting compatibility, and safety; the AI helper remains opt-in and isolated behind a narrow shim so the REPL path stays fast and predictable.

## Mission
- Deliver a minimal, reliable POSIX shell core with strategy-based builtins and traditional pipeline execution.
- Provide an ergonomic, tab-confirmable LLM assistant that never blocks the shell if unavailable.
- Preserve a development workflow that mirrors typical terminal usage: compile with standard toolchains, run locally, avoid external services.

## Feature Highlights
- Interactive shell with `PATH` lookup, pipes, basic redirections, and environment/tilde expansion.
- Builtins dispatch table covering `cd`, `exit`, `pwd`, `echo`, `export`, `unset`, and `umask`, each implemented in its own translation unit.
- Optional Gemma-based helper (`gemma_cli`) via llama.cpp, with safety hooks for prompt filtering and logit biasing.
- Portable build scripts for macOS (Metal) and Linux/Windows (CPU-only) targets.

## Quick Start
### Prerequisites
- Clang or GCC with C17 support (helper scripts invoke Clang by default).
- `cmake` when compiling the bundled llama.cpp extras.
- macOS (arm64, Metal) or Linux (x86_64/arm64). The shell itself is portable; the LLM sidecar depends on platform acceleration flags.
- Optional: Python 3.9+ and `pip` for GGUF tooling, `git` for model weight downloads.

After cloning the repository, initialise the llama.cpp submodule when you need the LLM components:
```bash
git submodule update --init --recursive
```

### Build the Shell
Use the platform helper scripts from the project root. They default to compiling only the shell (`bin/genshell`). Pass the `all` target when you also require the LLM demo executable.

**macOS / Unix-like**
```bash
./build_mac.sh          # builds bin/genshell
./build_mac.sh all      # builds shell + bin/gemma_cli (expects llama.cpp static libs)
```

**Linux / Windows (MSYS/WSL)**
```bash
./build_pc.sh           # builds bin/genshell
./build_pc.sh all       # builds shell + bin/gemma_cli (expects llama.cpp static libs)
```

The scripts respect `CC`, `CXX`, `SHELL_CFLAGS`, `LLM_CFLAGS`, and `LLM_CXXFLAGS` if you need to override compilers or warning levels. Object files land in `build/obj/`, artefacts in `bin/`.

To build the llama.cpp static libraries first, use the `*_complete` helpers which run CMake and then invoke the corresponding build script with the `all` target:
```bash
./build_mac_complete.sh   # macOS Metal toolchain + both binaries
./build_pc_complete.sh    # CPU-only toolchain + both binaries
```

### Run GenShell
Launch the shell directly once `bin/genshell` exists:
```bash
./bin/genshell
```

Current limitations:
- Pipelines always run in the foreground; job control is not yet implemented.
- Only basic redirection operators are available; descriptors such as `2>&1` and here-documents remain TODO.
- Word splitting, command substitution, arithmetic expansion, and shell functions are future work.

## Repository Layout
```text
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

## Where to Next?
- Follow the detailed build and platform guidance in [Guides](guides/build.md).
- Explore development practices, formatting, and testing expectations in [Development Practices](development/practices.md).
- Review contribution rules and the repository process in [Contributor Guide](contributing/contributor-guide.md).
- Learn about the LLM helper, sampling safety, and prompt pipeline in [LLM Sidecar](guides/llm-sidecar.md).
