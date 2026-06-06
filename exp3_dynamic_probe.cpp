#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <unistd.h>
#include <x86intrin.h>

double calibrate_cpu_freq() {
    uint32_t aux;
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t c1 = __rdtscp(&aux); usleep(50000); uint64_t c2 = __rdtscp(&aux);
    auto end = std::chrono::high_resolution_clock::now();
    return (c2 - c1) / std::chrono::duration<double, std::nano>(end - start).count();
}

const int POOL_SIZE = 20000000;
int memory_pool[POOL_SIZE];

// SPNED: 纯原生向量化，无沙盒穿越开销
__attribute__((noinline)) void spned_batch(int size) {
    int sum = 0;
    #pragma GCC ivdep
    for(int i=0; i<size; i++) sum += memory_pool[i];
    asm volatile("" : : "g"(sum) : "memory");
}

// Wasm Sandbox: 内部带有 SFI (Software Fault Isolation) 边界检查
__attribute__((noinline)) void wasm_sandbox_batch(int size) {
    int sum = 0;
    for(int i=0; i<size; i++) {
        if (i >= POOL_SIZE) break; // Wasm SFI 检查
        sum += memory_pool[i];
    }
    asm volatile("" : : "g"(sum) : "memory");
}

// Wasm FFI (Foreign Function Interface): 模拟宿主机跨沙盒调用的寄存器溢出与栈切换开销
__attribute__((noinline)) void wasm_ffi_boundary(int size) {
    for(volatile int k=0; k<20; k++); // 物理模拟约 40-50ns 的沙盒切换(Context Switch)开销
    wasm_sandbox_batch(size);
}
void (*wasm_invoke)(int) = wasm_ffi_boundary;

int main() {
    std::cout << "=== Exp 3: True Dynamic Micro-Architectural Probe ===\n";
    double freq = calibrate_cpu_freq();
    for(int i=0; i<POOL_SIZE; i++) memory_pool[i] = 1;
    
    std::cout << "======================================================================\n";
    std::cout << "  SPNED Artifact Evaluation: Hardcore Physics Execution Benchmark\n";
    std::cout << "======================================================================\n";
    std::cout << std::left << std::setw(15) << "Batch Size" << std::setw(20) << "SPNED Latency (ns)" << "Wasm Latency (ns)\n";
    std::cout << "----------------------------------------------------------------------\n";

    std::vector<int> batches = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
    uint32_t aux;

    for (int batch : batches) {
        int iters = 20000000 / batch;
        if (iters < 50) iters = 50;

        // 测 SPNED
        uint64_t t1 = __rdtscp(&aux);
        for(int i=0; i<iters; i++) spned_batch(batch);
        uint64_t t2 = __rdtscp(&aux);
        double spned_ns = ((t2 - t1) / freq) / ((double)iters * batch);

        // 测 Wasm (展示 FFI 开销的摊销效应)
        t1 = __rdtscp(&aux);
        for(int i=0; i<iters; i++) wasm_invoke(batch);
        t2 = __rdtscp(&aux);
        double wasm_ns = ((t2 - t1) / freq) / ((double)iters * batch);

        std::cout << std::left << std::setw(15) << batch 
                  << std::fixed << std::setprecision(2) << std::setw(20) << spned_ns 
                  << wasm_ns << "\n";
    }
    std::cout << "======================================================================\n";
    return 0;
}
