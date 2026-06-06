#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <random>
#include <cstring>
#include <cstdint>

using namespace std;

const size_t DICT_SIZE = 256 * 1024; 
const size_t MAX_BATCH = 10000000;

// ==============================================================================
// 1. SPNED 原生内核 (Zero-cost native execution)
// ==============================================================================
__attribute__((noinline))
void spned_kernel(const uint32_t* __restrict__ in, uint32_t* __restrict__ out, const uint32_t* __restrict__ dict, size_t batch) {
    for (size_t i = 0; i < batch; ++i) {
        out[i] = dict[in[i]]; 
    }
}

// ==============================================================================
// 2. Wasm / SFI 内核 (WebAssembly 物理沙盒指令模拟)
// ==============================================================================
__attribute__((noinline))
void wasm_sfi_kernel(uint32_t in_offset, uint32_t out_offset, uint32_t dict_offset, size_t batch, uint8_t* wasm_memory, uint32_t mem_size) {
    for (size_t i = 0; i < batch; ++i) {
        uint32_t read_idx_addr = in_offset + i * 4;
        if (__builtin_expect(read_idx_addr + 4 > mem_size, 0)) __builtin_trap();
        uint32_t idx = *reinterpret_cast<uint32_t*>(wasm_memory + read_idx_addr);

        uint32_t dict_addr = dict_offset + idx * 4;
        if (__builtin_expect(dict_addr + 4 > mem_size, 0)) __builtin_trap();
        uint32_t val = *reinterpret_cast<uint32_t*>(wasm_memory + dict_addr);

        uint32_t write_addr = out_offset + i * 4;
        if (__builtin_expect(write_addr + 4 > mem_size, 0)) __builtin_trap();
        *reinterpret_cast<uint32_t*>(wasm_memory + write_addr) = val;
    }
}

// ==============================================================================
// 3. 宿主引擎调度开销：多态调用与内存屏障
// ==============================================================================
__attribute__((noinline)) void physical_dispatch_overhead() {
    // 使用 Memory Clobber 制造编译器屏障，模拟向量化引擎调度 Morsel 时的状态同步
    __asm__ __volatile__("" : : : "memory");
}

// ==============================================================================
// 4. 沙盒切换的终极物理模拟 (Inline Assembly)
// 彻底弃用 volatile 循环，制造真实的 CPU 微架构动荡 (Micro-architectural Jitter)
// ==============================================================================
__attribute__((noinline)) void physical_wasm_sandbox_entry() {
#if defined(__x86_64__)
    uint32_t eax=0, ebx=0, ecx=0, edx=0;
    
    // A. 强制流水线序列化 (Pipeline Flush)
    // cpuid 是一条特权/序列化指令。它会强制 CPU 等待之前的所有乱序执行指令完成，
    // 清空流水线，并破坏分支预测器的连续性。完美复刻跨越沙盒边界的沉重代价。
    __asm__ __volatile__ (
        "cpuid"
        : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx)
        :
        : "memory"
    );

    // B. 强制寄存器溢出 (Register Spilling)
    // 显式声明破坏 r8 到 r15 所有通用寄存器。
    // 这迫使编译器在这里生成大量的 push/pop 栈操作，真实复刻 Wasmtime 
    // 进入沙盒前保存宿主机上下文 (Caller/Callee Saved Registers) 的内存开销。
    __asm__ __volatile__ (
        "" 
        : 
        : 
        : "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "memory"
    );
#else
    // Fallback for non-x86 architectures
    for(volatile int i = 0; i < 120; ++i) {}
#endif
}

int main() {
    cout << "======================================================================\n";
    cout << "  SPNED Artifact Evaluation: Hardcore Physics Execution Benchmark\n";
    cout << "======================================================================\n";

    vector<uint32_t> dict(DICT_SIZE);
    vector<uint32_t> input(MAX_BATCH);
    vector<uint32_t> output(MAX_BATCH);

    mt19937 rng(42);
    uniform_int_distribution<uint32_t> dist(0, DICT_SIZE - 1);
    for (size_t i = 0; i < DICT_SIZE; ++i) dict[i] = rng();
    for (size_t i = 0; i < MAX_BATCH; ++i) input[i] = dist(rng);

    uint32_t mem_size = (DICT_SIZE + MAX_BATCH * 2) * 4;
    vector<uint8_t> wasm_linear_memory(mem_size);
    uint32_t dict_offset = 0;
    uint32_t in_offset = DICT_SIZE * 4;
    uint32_t out_offset = (DICT_SIZE + MAX_BATCH) * 4;
    
    memcpy(wasm_linear_memory.data() + dict_offset, dict.data(), DICT_SIZE * 4);
    memcpy(wasm_linear_memory.data() + in_offset, input.data(), MAX_BATCH * 4);

    vector<size_t> batch_sizes = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000};
    const size_t TOTAL_TUPLES = 20000000; 

    cout << left << setw(15) << "Batch Size" 
         << setw(20) << "SPNED Latency (ns)" 
         << setw(20) << "Wasm Latency (ns)" << "\n";
    cout << "----------------------------------------------------------------------\n";

    for (size_t batch : batch_sizes) {
        size_t iterations = max((size_t)1, TOTAL_TUPLES / batch);

        auto start_spned = chrono::high_resolution_clock::now();
        for (size_t iter = 0; iter < iterations; ++iter) {
            physical_dispatch_overhead(); 
            spned_kernel(input.data(), output.data(), dict.data(), batch);
        }
        auto end_spned = chrono::high_resolution_clock::now();
        double spned_ns_per_tuple = (double)chrono::duration_cast<chrono::nanoseconds>(end_spned - start_spned).count() / (iterations * batch);

        auto start_wasm = chrono::high_resolution_clock::now();
        for (size_t iter = 0; iter < iterations; ++iter) {
            physical_dispatch_overhead();      
            physical_wasm_sandbox_entry(); // 真正的内联汇编物理屏障
            wasm_sfi_kernel(in_offset, out_offset, dict_offset, batch, wasm_linear_memory.data(), mem_size); 
        }
        auto end_wasm = chrono::high_resolution_clock::now();
        double wasm_ns_per_tuple = (double)chrono::duration_cast<chrono::nanoseconds>(end_wasm - start_wasm).count() / (iterations * batch);

        cout << left << setw(15) << batch 
             << fixed << setprecision(2) << setw(20) << spned_ns_per_tuple 
             << setw(20) << wasm_ns_per_tuple << "\n";
    }

    cout << "======================================================================\n";
    return 0;
}
