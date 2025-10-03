#!/bin/bash
rm -rf build/obj/*  

clang  -std=c17  \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_cli.c -o build/obj/gemma_cli.o
clang++ -std=c++20 \
    -Iinclude -I"deps/llama.cpp" -I"deps/llama.cpp/include" -I"deps/llama.cpp/ggml/include" \
    -c src/gemma_llama.cpp -o build/obj/gemma_llama.o
clang++ -std=c++20 \
    build/obj/gemma_llama.o build/obj/gemma_cli.o \
    deps/llama.cpp/build/src/libllama.a \
    deps/llama.cpp/build/ggml/src/libggml.a \
    deps/llama.cpp/build/ggml/src/libggml-base.a \
    deps/llama.cpp/build/ggml/src/libggml-cpu.a \
    deps/llama.cpp/build/ggml/src/ggml-blas/libggml-blas.a \
    deps/llama.cpp/build/ggml/src/ggml-metal/libggml-metal.a \
    -framework Accelerate -framework Metal -framework MetalKit \
    -framework Foundation -framework QuartzCore -lobjc \
    -o bin/gemma_cli