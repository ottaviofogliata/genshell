# Contributor Guide

## Mission in One Paragraph
GenShell is a ground-up POSIX shell written in C with an opt-in, local LLM sidecar powered by llama.cpp. The default profile targets Qwen2.5-Coder-3B-Instruct, with Gemma retained for legacy validation. The shell must remain fast, script-compatible, and safe even when the model is unavailable. LLM features live behind a narrow shim and never take over core shell responsibilities.

## Expected User Experience
A classifier routes natural-language queries to the LLM helper while traditional shell commands run immediately. Suggestions are rendered in the CLI and require an explicit `TAB` confirmation before execution. See [LLM Sidecar](../guides/llm-sidecar.md) for the full pipeline diagram and safety defaults, and `LLM_DOWNLOAD.md` at the repo root for download/quantization steps.

## Coding Expectations
- C files (`*.c`) compile as C17; C++ files (`*.cpp`) compile as C++20.
- Follow clang-format (style file TBD). Run the formatter over every touched file before submitting patches.
- No exceptions; return `0` on success, negative on failure (propagate `errno` semantics whenever practical).
- Document ownership in headers: clearly describe who allocates and who frees.
- Keep functions focused (roughly < 80 LOC). Refactor into helpers when they grow.

## Safety & Sampling Defaults
- Default prompt sampling is tuned for balanced chat; review the long comment block in `gemma_cli.c` (wired to Qwen2.5-Coder-3B-Instruct by default) before altering behaviour.
- The logit-bias safety list (e.g., `rm -rf /`) lives in `gemma_cli.c`. Extend or override it per deployment.
- When exposing new model features, add switches to `gemma_sampling_config` and document them alongside existing parameters.

## Testing & Verification
- Manual testing and the demo CLI are the only harnesses today.
- Planned CI work: unit tests for the shim, an integration harness for shell + LLM flows, and static analysis.
- Until automated coverage lands, double-check builds on macOS (Metal) and Linux CPU before merging.
- Keep each test file focused: add a brief file-level comment describing coverage and annotate individual tests with concise intent statements.

## Contribution Checklist
- Confirm instructions in README, architecture docs, and this site remain accurate after changes.
- Keep llama.cpp as a clean submodule (no downstream patches unless upstreamed).
- Verify `bin/gemma_cli` runs end-to-end on at least one platform when touching LLM code, using the Qwen2.5-Coder-3B-Instruct checkpoint unless you are explicitly testing legacy Gemma flows.
- Run clang-format on modified files.
- Update this guide or architecture docs when adjusting build flow or directory layout.

## Notes Log Expectations
- Log all meaningful repository changes in `NOTES.md` immediately after they land.
- Use the format `[YYYY-MM-DD HH:MM:SS] > narrative`, adding entries to the top of the file.
- Capture what changed, why, follow-up actions, and any context future contributors need.
- Separate entries with a blank line. Treat missing or vague entries as a process failure.

Refer to `.docs/refs/` for additional background material.
