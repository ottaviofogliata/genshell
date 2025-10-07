#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

OBJ_DIR="build/obj"
BIN_DIR="bin"
mkdir -p "$OBJ_DIR" "$BIN_DIR"

CC=${CC:-clang}
CXX=${CXX:-clang++}
SHELL_CFLAGS=${SHELL_CFLAGS:--std=c17 -Wall -Wextra -Wno-unused-parameter}
LLM_CFLAGS=${LLM_CFLAGS:--std=c17 -Wall -Wextra -Wno-unused-parameter}
LLM_CXXFLAGS=${LLM_CXXFLAGS:--std=c++20 -Wall -Wextra -Wno-unused-parameter}

SHELL_SOURCES=(
    src/kernel/shell/main.c
    src/kernel/shell/shell.c
    src/kernel/shell/parser/lexer.c
    src/kernel/shell/parser/parser.c
    src/kernel/shell/exec/executor.c
    src/kernel/shell/builtins/registry.c
    src/kernel/shell/builtins/cd.c
    src/kernel/shell/builtins/echo.c
    src/kernel/shell/builtins/exit.c
    src/kernel/shell/builtins/export.c
    src/kernel/shell/builtins/pwd.c
    src/kernel/shell/builtins/umask.c
    src/kernel/shell/builtins/unset.c
)

LLM_SOURCES=(
    src/llm/gemma_cli.c
    src/llm/gemma_runner.c
    src/llm/llm_chat_template.c
    src/llm/command_stream_parser.c
    src/llm/templates/qwen_chat_template.c
)

obj_name() {
    local src="$1"
    local stripped="${src%.c}"
    echo "$OBJ_DIR/$(echo "$stripped" | tr '/' '_').o"
}

build_shell() {
    echo "==> Building genshell"
    local shell_objects=()
    for src in "${SHELL_SOURCES[@]}"; do
        local obj
        obj=$(obj_name "$src")
        "$CC" $SHELL_CFLAGS -Isrc/kernel/shell -Iinclude -c "$src" -o "$obj"
        shell_objects+=("$obj")
    done
    "$CC" -std=c17 -o "$BIN_DIR/genshell" "${shell_objects[@]}"
    echo "Built $BIN_DIR/genshell"
}

build_gemma_cli() {
    echo "==> Building gemma_cli"
    local llm_objects=()
    for src in "${LLM_SOURCES[@]}"; do
        local obj
        obj=$(obj_name "$src")
        "$CC" $LLM_CFLAGS \
            -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
            -c "$src" -o "$obj"
        llm_objects+=("$obj")
    done
    local gemma_llama_obj
    gemma_llama_obj=$(obj_name "src/llm/gemma_llama.cpp")

    "$CXX" $LLM_CXXFLAGS \
        -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
        -c src/llm/gemma_llama.cpp -o "$gemma_llama_obj"

    "$CXX" -std=c++20 \
        "$gemma_llama_obj" "${llm_objects[@]}" \
        deps/llama.cpp/build/src/libllama.a \
        deps/llama.cpp/build/ggml/src/libggml.a \
        deps/llama.cpp/build/ggml/src/libggml-base.a \
        deps/llama.cpp/build/ggml/src/libggml-cpu.a \
        deps/llama.cpp/build/ggml/src/ggml-blas/libggml-blas.a \
        -lpthread -ldl \
        -o "$BIN_DIR/gemma_cli"
    echo "Built $BIN_DIR/gemma_cli"
}

usage() {
    cat <<USAGE
Usage: ./build_pc.sh [shell|llm|all]
  shell  Build only the POSIX shell executable (default)
  llm    Build only the Gemma llama.cpp demo CLI
  all    Build both artifacts
USAGE
}

TARGET=${1:-shell}
case "$TARGET" in
    shell)
        build_shell
        ;;
    llm)
        build_gemma_cli
        ;;
    all)
        build_shell
        build_gemma_cli
        ;;
    *)
        usage >&2
        exit 1
        ;;
esac
