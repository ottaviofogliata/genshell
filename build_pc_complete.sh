#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$ROOT_DIR/deps/llama.cpp"
cmake -S . -B build -DGGML_METAL=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target llama llama-quantize

cd "$ROOT_DIR"
./build_pc.sh all
