#!/bin/bash
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "conda" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (100% True Physical)"
echo "========================================================"

# 确保 Wasmtime C API 存在
if [ ! -d "wasmtime-c-api" ]; then
    wget -q https://github.com/bytecodealliance/wasmtime/releases/download/v14.0.4/wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
    tar xf wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz && mv wasmtime-v14.0.4-x86_64-linux-c-api wasmtime-c-api
    rm wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
fi

echo -e "\n>>> 1. Compiling & Executing Exp 1: TRUE Throughput Benchmark..."
g++ -O3 -std=c++14 measure_exp1_ultimate.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -o measure_exp1_bin
./measure_exp1_bin

echo -e "\n>>> 2. Compiling & Executing Exp 4: TRUE NUMA Scalability Benchmark..."
g++ -O3 -pthread exp4_numa_scaling.cpp -o exp4_numa_scaling_bin
./exp4_numa_scaling_bin
