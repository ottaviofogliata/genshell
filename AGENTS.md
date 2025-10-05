# GenShell – Contributor Guide

[./README.md](./README.md)

## 1. Mission in One Paragraph
GenShell is a ground-up POSIX shell written in C with an opt-in, local LLM sidecar powered by SLM (gemma 3 actually) through llama.cpp. The shell must remain fast, script-compatible, and safe even when the model is unavailable. LLM features live behind a narrow shim and never bleed into core shell responsibilities.

### Expected User Experience: Operational Flow and LLM Contract (Pure CLI Mode)

The flow is optimized for traditional terminal speed:

```mermaid
graph TD
    A["User Input"] --> B{Classifier};
    B -- "Natural Language" --> C[Prompt Structuring];
    C --> D[Local Inference];
    D --> E[JSON Output];
    E --> F[CLI Visualization];
    F --> G["User Accepts (TAB)"];
    G --> H["Execute Command"];
    B -- "Shell Command" --> H;

---

## 2. Coding Expectations (current repo scope)
- C files (`*.c`) compile as C17; C++ files (`*.cpp`) as C++20.
- Follow clang-format (style file TBD). Run `clang-format` over touched files before commit.
- No exceptions; return `0` on success, negative on failure (propagate `errno` semantics where possible).
- Document ownership in headers (who allocates/frees).
- Keep functions focused (< ~80 LOC); refactor into helpers if they grow.

---

## 3. Safety & Sampling Defaults
- Default prompt sampling is tuned for balanced chat (see the long comment block in `gemma_cli.c`).
- Logit bias safety list (e.g., `rm -rf /`) lives in `gemma_cli.c`; expand or override per deployment.
- When exposing new model features, add knobs to `gemma_sampling_config` and document them alongside the existing parameters.

---

## 4. Testing & Verification
- At present only manual testing and the demo CLI exist.
- CI TODOs: unit tests for the shim, integration harness for shell + LLM pipeline, static analysis.
- Until tests land, double-check builds on macOS (Metal) and Linux CPU before merging.
- Keep each test file focused: add a brief file-level comment summarizing coverage and annotate individual unit tests with short descriptions so intent stays obvious.

---

## 5. Contribution Checklist
- Confirm instructions in README/ARCH remain truthful.
- Keep llama.cpp as a clean submodule (no local edits unless upstreamed).
- Verify `bin/gemma_cli` runs end-to-end on at least one platform.
- Run clang-format on modified files.
- Update this file or ARCH.md when changing architecture, build flow, or directory layout.

---

## 6. Notes Log
- Log all meaningful repository changes in [NOTES.md](./NOTES.md) immediately after they land.
- Use lines shaped like `[YYYY-MM-DD HH:MM:SS] > narrative` with the newest entry at the top.
- Capture what changed, why it changed, follow-up actions, and any context future contributors need.
- Treat missing or vague entries as a process failure; the notes ledger is part of code review expectations.
- Each entry in the log must be separated by a blank line.

---

Refer to [./.docs/refs/](./.docs/refs/) for inspiration.

EOF.
