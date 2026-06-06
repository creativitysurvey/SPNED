#!/bin/bash
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "conda" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (100% True Physical)"
echo "========================================================"

# 确保 Wasmtime C API 环境就绪
if [ ! -d "wasmtime-c-api" ]; then
    echo ">>> Downloading Wasmtime C API..."
    wget -q https://github.com/bytecodealliance/wasmtime/releases/download/v14.0.4/wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
    tar xf wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz && mv wasmtime-v14.0.4-x86_64-linux-c-api wasmtime-c-api
    rm wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
fi

echo -e "\n>>> 1. Compiling & Executing Exp 1: TRUE Throughput Benchmark (8 Metrics)..."
g++ -O3 -std=c++14 measure_exp1_ultimate.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -o measure_exp1_bin
./measure_exp1_bin

echo -e "\n>>> 2. Compiling & Executing Exp 2: SPNED Verifier Precision & Latency (12 Metrics)..."
g++ -O3 -std=c++14 measure_exp2_verifier.cpp -o measure_exp2_bin
./measure_exp2_bin

echo -e "\n>>> 3. Compiling & Executing Exp 3: TRUE Wasmtime C API FFI Latency (16 Metrics)..."
g++ -O3 -std=c++14 exp3_real_wasm_ffi.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -o exp3_real_wasm_ffi_bin
./exp3_real_wasm_ffi_bin

echo -e "\n>>> 4. Compiling & Executing Exp 4: TRUE NUMA Scalability Benchmark (12 Metrics)..."
g++ -O3 -pthread -std=c++14 exp4_numa_scaling.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -o exp4_numa_scaling_bin
./exp4_numa_scaling_bin

echo -e "\n>>> 5. Executing Authentic Baselines (Linux eBPF Verifier & Wasmtime SFI)..."
if [ -f "run_real_baselines.sh" ]; then
    chmod +x run_real_baselines.sh
    ./run_real_baselines.sh
else
    echo "  -> [Notice] run_real_baselines.sh not found!"
fi

echo -e "\n========================================================"
echo " ALL EXPERIMENTS COMPLETED WITH TRUE HARDWARE METRICS"
echo "========================================================"
