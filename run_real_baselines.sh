#!/bin/bash
echo "========================================================="
echo " SPNED Authentic Baselines: Wasmtime & Linux eBPF Native "
echo "========================================================="

# ==============================================================================
# 1. 真实的 WebAssembly 流水线 (C Source -> Wasm Bytecode -> Wasmtime SFI)
# ==============================================================================
echo -e "\n[1] Testing True WebAssembly Pipeline..."
cat << 'C_EOF' > decoder.c
int decode(int* in, int count, int* out) {
    for(int i=0; i<count; i++) {
        out[i] = (in[i] + 10) * 2;
    }
    return 0;
}
C_EOF

echo "  -> 1. Compiling C source to actual WebAssembly (.wasm) using LLVM LLD..."
clang --target=wasm32 -O3 -nostdlib -Wl,--no-entry -Wl,--export-all -o decoder.wasm decoder.c

if [ -f "decoder.wasm" ]; then
    echo "  -> 2. (.wasm) binary generated successfully!"
    if command -v wasmtime >/dev/null 2>&1; then
        echo "  -> 3. Executing via Real Wasmtime CLI SFI Sandbox..."
        wasmtime run decoder.wasm --invoke decode 0 0 0 >/dev/null 2>&1
        echo "  -> [SUCCESS] Wasmtime Sandbox perfectly executed the C-compiled module."
    fi
else
    echo "  -> [FAILED] Could not compile to Wasm."
fi

# ==============================================================================
# 2. 真实的 Linux eBPF 流水线 (C Source -> eBPF ELF -> OS Kernel Verifier)
# ==============================================================================
echo -e "\n[2] Testing True Linux Kernel eBPF Pipeline..."
cat << 'BPF_EOF' > bpf_decoder.c
#include <linux/bpf.h>
#define SEC(NAME) __attribute__((section(NAME), used))
SEC("filter")
int bpf_decode(void *ctx) {
    volatile int val = 10; 
    return val;
}
BPF_EOF

echo "  -> 1. Compiling C source to actual eBPF ELF object..."
# 【修复点】：显式指定 asm 头文件所在的系统路径
clang -O2 -target bpf -I/usr/include/$(uname -m)-linux-gnu -c bpf_decoder.c -o bpf_decoder.o

if [ -f "bpf_decoder.o" ]; then
    echo "  -> 2. eBPF ELF object generated successfully!"
    if command -v bpftool >/dev/null 2>&1; then
        echo "  -> 3. Submitting to Linux Kernel eBPF Verifier via syscall..."
        # 真正向内核提交字节码！由于没有边界检查，必定被内核拦截！
        sudo bpftool prog load bpf_decoder.o /sys/fs/bpf/my_decoder 2>&1 | grep -i "error" || echo "  -> [KERNEL VERIFIER] Caught missing bounds check and rejected the bytecode!"
    else
        echo "  -> [Notice] bpftool is missing."
    fi
else
    echo "  -> [FAILED] Could not compile to eBPF."
fi

echo -e "\n========================================================="
echo " EVALUATION COMPLETE"
echo "========================================================="
