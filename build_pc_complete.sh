#!/bin/bash
cd deps/llama.cpp
cmake -S . -B build -DGGML_METAL=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target llama llama-quantize

cd ../..
./build_pc.sh