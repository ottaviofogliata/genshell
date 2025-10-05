# Development Practices

## Coding Standards
- Compile C sources as C17 and C++ sources as C++20.
- Avoid exceptions; functions should return `0` on success and negative values for failures, propagating `errno` semantics when possible.
- Keep functions focused (~80 LOC). Split helpers aggressively to preserve readability.
- Document ownership in headers: clarify which side allocates and frees resources.

## Formatting
- Run `clang-format` on every modified source file before committing. Add the binary to your toolchain if it is missing locally.
- Prefer ASCII in source and documentation unless non-ASCII characters are required.

## Notes & Process Hygiene
- Log every meaningful change in `NOTES.md`. Entries use the format `[YYYY-MM-DD HH:MM:SS] > narrative` with the newest entry at the top and a blank line between records.
- Treat missing or vague log entries as a process failure. The ledger is part of code review expectations.

## Testing & Verification
- Automated tests are still minimal. Run the smoke harness:
  ```bash
  ./tests/run_tests.sh
  ```
- Manually exercise the shell on macOS and Linux prior to merging significant changes.
- Planned CI additions include unit tests for the LLM shim, an integration harness covering shell + LLM flows, and static analysis runs.
- Give each new test file a brief top-level comment summarising coverage. Annotate individual tests with short descriptions.

## Contribution Checklist
- Ensure README, architecture docs, and this site stay accurate when behaviour or layout changes.
- Keep the llama.cpp submodule clean—no local edits unless upstreamed.
- Verify `bin/gemma_cli` runs end-to-end on at least one platform after touching LLM integration.
- Run `clang-format` on touched files.
- Update `AGENTS.md` (mirrored here) or `ARCH.md` if architecture or workflows change.

See [Contributor Guide](../contributing/contributor-guide.md) for broader responsibilities and culture notes.
