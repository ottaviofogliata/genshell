#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$repo_root/build/tests"
mkdir -p "$build_dir"

CC=${CC:-cc}
CFLAGS=${CFLAGS:-"-std=c17 -Wall -Wextra -Werror"}

EXTRA_CFLAGS="-D_POSIX_C_SOURCE=200809L"
if [[ "$(uname)" == "Darwin" ]]; then
    EXTRA_CFLAGS+=" -D_DARWIN_C_SOURCE"
fi

gemma_cli_bin="$build_dir/gemma_cli_stub"
yaml_test_bin="$build_dir/ctx_yaml_tests"

$CC $CFLAGS $EXTRA_CFLAGS \
    -I"$repo_root/include" \
    "$repo_root/src/llm/gemma_cli.c" \
    "$repo_root/tests/gemma_cli/stub_gemma_llama.c" \
    -o "$gemma_cli_bin"

log_file=$(mktemp)
trap 'rm -f "$log_file"' EXIT
export GEMMA_LLM_STUB_LOG="$log_file"

pass() {
    printf '✔ %s\n' "$1"
}

# Validates the default prompt fallback path for gemma_cli.
run_default_prompt_test() {
    : >"$log_file"
    local err_file="$build_dir/default_stderr.txt"
    local out_file="$build_dir/default_stdout.txt"

    "$gemma_cli_bin" >"$out_file" 2>"$err_file"

    grep -q "Usage:" "$err_file"
    grep -q "Falling back to default prompt" "$err_file"
    grep -q "Model: models/gemma-3-text-4b-it-4bit.gguf" "$out_file"
    grep -q "Prompt: Write a haiku about shell automation." "$out_file"
    grep -q "stub-token" "$out_file"

    grep -q "MODEL=models/gemma-3-text-4b-it-4bit.gguf" "$log_file"
    grep -q "PROMPT=Write a haiku about shell automation." "$log_file"

    rm -f "$err_file" "$out_file"
    pass "default prompt path"
}

# Confirms that explicit model path and prompt arguments flow through.
run_custom_arguments_test() {
    : >"$log_file"
    local out_file="$build_dir/custom_stdout.txt"
    local err_file="$build_dir/custom_stderr.txt"

    "$gemma_cli_bin" custom-model.gguf "Say hi" >"$out_file" 2>"$err_file"

    test ! -s "$err_file"
    grep -q "Model: custom-model.gguf" "$out_file"
    grep -q "Prompt: Say hi" "$out_file"
    grep -q "MODEL=custom-model.gguf" "$log_file"
    grep -q "PROMPT=Say hi" "$log_file"

    rm -f "$err_file" "$out_file"
    pass "custom model and prompt"
}

run_default_prompt_test
run_custom_arguments_test

$CC $CFLAGS $EXTRA_CFLAGS \
    -I"$repo_root/include" \
    "$repo_root/tests/ctx/test_yaml.c" \
    "$repo_root/src/ctx/library/yaml_parser.c" \
    -o "$yaml_test_bin"

# Executes the ctx_yaml unit tests covering string parsing and file loading.
export CTX_YAML_TEST_TMPDIR="$build_dir"
"$yaml_test_bin"
pass "ctx_yaml parser"

rm -f "$gemma_cli_bin" "$yaml_test_bin"
rmdir "$build_dir" 2>/dev/null || true

printf '\nAll tests passed.\n'
