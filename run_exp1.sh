#!/bin/bash
echo ">>> Compiling Experiment 1 Kernel Extractor (-O3 enabled)..."
# 使用 g++ 并开启 O3 和平台 AVX 优化，确保物理差异能够被释放
g++ -O3 -march=native -std=c++17 measure_exp1.cpp -o measure_exp1_bin

if [ $? -eq 0 ]; then
    echo ">>> Compilation successful. Executing physical throughput analysis..."
    echo ""
    ./measure_exp1_bin
else
    echo "[-] Compilation failed."
fi
