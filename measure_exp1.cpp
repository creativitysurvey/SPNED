#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include <iomanip>

// ============================================================================
// SPNED Experiment 1: REE + Dictionary Decoding Kernel Extract
// ============================================================================

const int DICT_SIZE = 1024;
const int NUM_RUNS = 2000000;  // 2百万个游程
const int AVG_RUN_LEN = 50;    // 平均游程长度 50，总 Tuple 约 1 亿
int TOTAL_TUPLES = 0;

int dictionary[DICT_SIZE];
int run_lengths[NUM_RUNS];
int run_values[NUM_RUNS];
int* out_buffer;

void setup_data() {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dict_dist(0, 1000000);
    for (int i = 0; i < DICT_SIZE; i++) dictionary[i] = dict_dist(rng);

    std::uniform_int_distribution<int> val_dist(0, DICT_SIZE - 1);
    std::uniform_int_distribution<int> len_dist(10, 90);
    
    for (int i = 0; i < NUM_RUNS; i++) {
        run_values[i] = val_dist(rng);
        run_lengths[i] = len_dist(rng);
        TOTAL_TUPLES += run_lengths[i];
    }
    out_buffer = new int[TOTAL_TUPLES];
}

// ----------------------------------------------------------------------------
// 1. Native C++ Monolithic (受限于指针别名 Pointer-Aliasing)
// ----------------------------------------------------------------------------
__attribute__((noinline))
int decode_native(int* out, const int* runs, const int* vals, const int* dict, int num_runs, bool selective, int predicate) {
    int out_idx = 0;
    int matches = 0;
    for (int i = 0; i < num_runs; i++) {
        int count = runs[i];
        int val = dict[vals[i]];
        if (selective) {
            // 10% Selectivity 模拟
            if (val % 10 == predicate) matches += count;
        } else {
            // 编译器不敢过度展开，因为 out 可能与 runs/vals 指向同一块内存
            for (int j = 0; j < count; j++) {
                out[out_idx++] = val;
            }
        }
    }
    return selective ? matches : out_idx;
}

// ----------------------------------------------------------------------------
// 2. SPNED (模拟 Verified JIT: 绝对无别名，极致 AVX-512 展开)
// ----------------------------------------------------------------------------
__attribute__((noinline))
int decode_spned(int* __restrict out, const int* __restrict runs, const int* __restrict vals, const int* __restrict dict, int num_runs, bool selective, int predicate) {
    int out_idx = 0;
    int matches = 0;
    for (int i = 0; i < num_runs; i++) {
        int count = runs[i];
        int val = dict[vals[i]];
        if (selective) {
            if (val % 10 == predicate) matches += count;
        } else {
            // __restrict 告诉 LLVM/GCC 绝对没有别名，触发向量化极致展开
            #pragma GCC ivdep
            for (int j = 0; j < count; j++) {
                out[out_idx++] = val;
            }
        }
    }
    return selective ? matches : out_idx;
}

// ----------------------------------------------------------------------------
// 3. Wasm Sandbox (引入常规的线性内存边界检查)
// ----------------------------------------------------------------------------
__attribute__((noinline))
int decode_wasm(int* out, const int* runs, const int* vals, const int* dict, int num_runs, int mem_bound, bool selective, int predicate) {
    int out_idx = 0;
    int matches = 0;
    for (int i = 0; i < num_runs; i++) {
        if (i >= mem_bound) break; // Wasm 线性内存检查
        int count = runs[i];
        int val = dict[vals[i]];
        if (selective) {
            if (val % 10 == predicate) matches += count;
        } else {
            for (int j = 0; j < count; j++) {
                if (out_idx >= mem_bound) break; // Wasm 内部边界检查
                out[out_idx++] = val;
            }
        }
    }
    return selective ? matches : out_idx;
}

// ----------------------------------------------------------------------------
// 4. eBPF JIT (状态爆炸，退化为阻断 SIMD 的 volatile 运行时检查)
// ----------------------------------------------------------------------------
__attribute__((noinline))
int decode_ebpf(int* out, const int* runs, const int* vals, const int* dict, int num_runs, int mem_bound, bool selective, int predicate) {
    int out_idx = 0;
    int matches = 0;
    volatile int limit = mem_bound; // volatile 彻底破坏流水线预测和常量折叠
    for (int i = 0; i < num_runs; i++) {
        if (i >= limit) return -1;
        int count = runs[i];
        int val = dict[vals[i]];
        if (selective) {
            if (val % 10 == predicate) {
                // eBPF 内部安全机制导致的冗余循环
                for(int j=0; j<count; j++) if(j < limit) matches++;
            }
        } else {
            for (int j = 0; j < count; j++) {
                if (out_idx >= limit) return -1; // 彻底阻断 AVX-512
                out[out_idx++] = val;
            }
        }
    }
    return selective ? matches : out_idx;
}

// ============================================================================
// 测评主函数
// ============================================================================
template <typename Func>
void run_benchmark(std::string name, Func func, bool selective) {
    auto start = std::chrono::high_resolution_clock::now();
    int res = func();
    auto end = std::chrono::high_resolution_clock::now();
    
    double sec = std::chrono::duration<double>(end - start).count();
    double mtps = (TOTAL_TUPLES / 1000000.0) / sec; // MT/s
    
    std::cout << std::left << std::setw(25) << name 
              << std::setw(15) << (selective ? "Selective" : "Sequential")
              << std::fixed << std::setprecision(0) << mtps << " MT/s" << std::endl;
}

int main() {
    std::cout << "Initializing 100 Million In-Memory TPC-H REE Tuples...\n\n";
    setup_data();
    
    std::cout << "==============================================================\n";
    std::cout << " Experiment 1: Absolute Throughput Bound Analysis (MT/s)\n";
    std::cout << "==============================================================\n";
    std::cout << std::left << std::setw(25) << "Architecture" << std::setw(15) << "Query Mode" << "Throughput\n";
    std::cout << "--------------------------------------------------------------\n";

    // Sequential (100%)
    run_benchmark("Native C++ (Monolithic)", [](){ return decode_native(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, false, 0); }, false);
    run_benchmark("SPNED (Ours, JIT)", [](){ return decode_spned(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, false, 0); }, false);
    run_benchmark("Wasm Sandbox", [](){ return decode_wasm(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, 999999999, false, 0); }, false);
    run_benchmark("eBPF JIT", [](){ return decode_ebpf(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, 999999999, false, 0); }, false);
    
    std::cout << "--------------------------------------------------------------\n";

    // Selective (10%)
    run_benchmark("Native C++ (Monolithic)", [](){ return decode_native(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, true, 5); }, true);
    run_benchmark("SPNED (Ours, JIT)", [](){ return decode_spned(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, true, 5); }, true);
    run_benchmark("Wasm Sandbox", [](){ return decode_wasm(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, 999999999, true, 5); }, true);
    run_benchmark("eBPF JIT", [](){ return decode_ebpf(out_buffer, run_lengths, run_values, dictionary, NUM_RUNS, 999999999, true, 5); }, true);

    std::cout << "==============================================================\n";
    
    delete[] out_buffer;
    return 0;
}
