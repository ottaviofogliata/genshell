[2025-10-07 22:08:42] > Refactored the LLM helper into focused modules: introduced a chat-template interface with a Qwen implementation, split the streaming JSON parser and llama runtime setup into reusable helpers, reworked gemma_cli.c to orchestrate those layers, and refreshed the build scripts to compile the new units.

[2025-10-07 21:45:33] > Added a repository-managed pre-commit hook that runs clang-format over staged C/C++ sources and restages them to keep formatting consistent.

[2025-10-07 21:36:47] > Updated src/llm/gemma_cli.c to build the Qwen2.5 chat template with a command-only system prompt, stream just the generated CLI commands from the JSON response, add a raw-output fallback when parsing fails, and switch the helper default to the Qwen checkpoint.

[2025-10-06 14:30:29] > Documented Qwen2.5-Coder-3B-Instruct as the default sidecar, moved LLM download + quantization steps into LLM_DOWNLOAD.md, and refreshed README/MkDocs to point at the consolidated guide.

[2025-10-05 19:33:09] > Added a MkDocs + Material documentation site (docs/ + mkdocs.yml), ported existing README and contributor guidance into structured pages, introduced a Doxygen config that outputs to docs/api for API reference, and documented the new workflow in README.md.

[2025-10-04 20:35:00] > Promoted the POSIX shell toolchain by rewriting build scripts (mac/pc) to default to genshell, adding optional llm targets, refreshing the complete helpers, and overhauled README.md to describe the new shell-first workflow, known gaps, and opt-in LLM steps.

[2025-10-04 20:12:34] > Landed the first genshell implementation: scaffolded the kernel shell runtime with REPL, lexer/parser, executor, and a strategy-builtins registry, split builtins into dedicated files (cd, exit, pwd, echo, export, unset, umask), and produced the `bin/genshell` binary; background jobs and advanced redirection remain TODO, and clang-format is missing locally so formatting should be run once available.

[2025-10-04 12:23:12] > Linked NOTES.md usage requirements into AGENTS.md §6 and trimmed the notes file intro so instructions live in one place; added reminder that NOTES entries are mandatory and updated the ledger accordingly.

[2025-10-04 12:20:23] > Brought the repo layout and tooling in line with contributor docs: moved LLM sources under `src/llm`, added placeholder READMEs for `kernel/`, `ctx/`, `infra/`, `tests/`, refreshed build scripts and README paths, introduced a dependency-free YAML parser in `src/ctx/library/`, expanded `tests/run_tests.sh` to cover both `gemma_cli` and the new parser, documented testing obligations in `AGENTS.md`, and set up GitHub Actions plus local instructions so contributors have a clear, enforceable workflow.
