#!/bin/bash
set -e # 开启严格模式，遇到任何错误立即退出

# ========================================================
# 0. 自动环境隔离 (免疫 Conda 等幽灵虚拟环境)
# ========================================================
export PATH=$(echo $PATH | tr ':' '\n' | grep -v "conda" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (100% True Physical)"
echo "========================================================"
echo "[+] Environment sanitized. Using absolute system toolchain."

# ========================================================
# 1. 自动依赖自举 (拉取并编译 Wasmtime 与 uBPF)
# ========================================================
# 确保 Wasmtime C API 环境就绪
if [ ! -d "wasmtime-c-api" ]; then
    echo ">>> Downloading Wasmtime C API..."
    wget -q https://github.com/bytecodealliance/wasmtime/releases/download/v14.0.4/wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
    tar xf wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz && mv wasmtime-v14.0.4-x86_64-linux-c-api wasmtime-c-api
    rm wasmtime-v14.0.4-x86_64-linux-c-api.tar.xz
fi

# 确保 IOVisor uBPF 虚拟机就绪并修复配置头文件
if [ ! -f "$(pwd)/ubpf/lib/libubpf.a" ] && [ ! -f "$(pwd)/ubpf/build/lib/libubpf.a" ]; then
    echo "[+] Bootstrapping IOVisor uBPF virtual machine..."
    git clone https://github.com/iovisor/ubpf.git || true
    cd ubpf
    git submodule update --init --recursive || true
    # 强制手写注入配置文件，防止 CMake 动态生成路径丢失
    mkdir -p vm/inc
    echo -e "#ifndef UBPF_CONFIG_H\n#define UBPF_CONFIG_H\n#define UBPF_HAS_JIT 1\n#endif" > vm/inc/ubpf_config.h
    sed -i 's/<ubpf_config.h>/"ubpf_config.h"/g' vm/inc/ubpf.h
    cmake -S . -B build
    make -C build
    cd ..
fi

# ========================================================
# 2. 自动载荷生成 (零依赖纯物理 eBPF 字节码)
# ========================================================
if [ ! -f "ebpf_seq.bin" ] || [ ! -f "ebpf_sel.bin" ]; then
    echo "[+] Generating zero-dependency bare-metal eBPF bytecodes..."
    
    # 生成 Sequential 载荷
    cat << 'EOF' > ebpf_seq.c
typedef unsigned long long uint64_t;
struct bpf_ctx { int* arr; uint64_t count; };
uint64_t func(struct bpf_ctx *ctx) {
    int* arr = ctx->arr;
    uint64_t count = ctx->count;
    for (uint64_t i = 0; i < 10000000; i++) {
        if (i >= count) break;
        arr[i] = (arr[i] + 10) * 2;
    }
    return 0;
}
EOF

    # 生成 Selective 载荷
    cat << 'EOF' > ebpf_sel.c
typedef unsigned long long uint64_t;
struct bpf_ctx { int* arr; uint64_t count; };
uint64_t func(struct bpf_ctx *ctx) {
    int* arr = ctx->arr;
    uint64_t count = ctx->count;
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 10000000; i++) {
        if (i >= count) break;
        if (arr[i] < 50) sum += arr[i];
    }
    return sum;
}
EOF

    # 交叉编译剥离标准库的纯净 ELF 对象
    clang -O2 -target bpf -c ebpf_seq.c -o ebpf_seq.elf
    clang -O2 -target bpf -c ebpf_sel.c -o ebpf_sel.elf
    
    # 暴力提取真实的 .text 机器码段
    llvm-objcopy -O binary -j .text ebpf_seq.elf ebpf_seq.bin
    llvm-objcopy -O binary -j .text ebpf_sel.elf ebpf_sel.bin
fi


# ========================================================
# 3. 核心实验套件执行 (动态捕获绝对物理路径)
# ========================================================
# 精准抓取生成的静态库绝对路径，替换掉原本硬编码的 /home/liwei/...
REAL_UBPF_LIB=$(find $(pwd)/ubpf -name "libubpf.a" | head -n 1)

echo -e "\n>>> 1. Compiling & Executing Exp 1: TRUE Throughput Benchmark (8 Metrics)..."
g++ -O3 -std=c++14 measure_exp1_ultimate.cpp -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib -I./ubpf/vm/inc ${REAL_UBPF_LIB} -o measure_exp1_bin -lm -lpthread -ldl -lrt
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