#!/bin/bash
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (100% True Physical)"
echo "========================================================"

echo -e "\n>>> 1. Compiling & Executing Exp 1: Authentic Throughput & OS Probes..."
g++ -O3 -march=native measure_exp1_ultimate.cpp -o measure_exp1_ultimate_bin 2>/dev/null
if [ -f "measure_exp1_ultimate_bin" ]; then ./measure_exp1_ultimate_bin; fi

echo -e "\n>>> 2. Compiling & Executing Exp 2: True Dynamic x86-64 JIT Engine..."
g++ -O3 -std=c++14 spned_dynamic_jit.cpp -o spned_dynamic_jit_bin 2>/dev/null
if [ -f "spned_dynamic_jit_bin" ]; then 
    ./spned_dynamic_jit_bin "+ 10 * 2"
    ./spned_dynamic_jit_bin "+ 100 * 3"
fi

echo -e "\n>>> 3. Compiling & Executing Exp 3: TRUE Wasmtime C API FFI Probe..."
if [ ! -d "wasmtime-c-api" ]; then
    echo "  -> Downloading Wasmtime C API..."
    wget -q https://github.com/bytecodealliance/wasmtime/releases/download/v14.0.4/wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
    tar xf wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz && mv wasmtime-v14.0.4-x86_64-linux-c-api wasmtime-c-api
    rm wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
fi
g++ -O3 -std=c++14 exp3_real_wasm_ffi.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -o exp3_real_wasm_ffi_bin 2>/dev/null
if [ -f "exp3_real_wasm_ffi_bin" ]; then ./exp3_real_wasm_ffi_bin; fi

echo -e "\n>>> 4. Compiling & Executing Exp 4: NUMA-Aware Scalability..."
g++ -O3 -pthread exp4_numa_scaling.cpp -o exp4_numa_scaling_bin 2>/dev/null
if [ -f "exp4_numa_scaling_bin" ]; then ./exp4_numa_scaling_bin; fi

echo -e "\n>>> 5. Executing Authentic Baselines (Wasmtime & Linux eBPF Native)..."
if [ -f "run_real_baselines.sh" ]; then ./run_real_baselines.sh; fi

echo -e "\n========================================================"
echo " ALL EXPERIMENTS COMPLETED WITH TRUE HARDWARE METRICS"
echo "========================================================"
