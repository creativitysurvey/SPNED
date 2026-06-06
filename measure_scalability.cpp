#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <iomanip>
#include <random>
#include <atomic>
#include <cstring>
#include <cstdint>
#include <pthread.h> // 必须引入 pthread 进行底层的 CPU 亲和性设置

using namespace std;

const size_t TOTAL_TUPLES = 500000000; 
const size_t DICT_SIZE = 256 * 1024;   
const size_t MORSEL_SIZE = 10000;

alignas(64) std::atomic<int> wasm_instance_ref_count{0};

// ==============================================================================
// 辅助函数：将当前线程强制绑定到指定的物理 CPU 核心上 (Thread Pinning)
// 彻底杜绝 Linux 调度器 (CFS) 在高压下产生的线程跨 NUMA 节点迁移。
// ==============================================================================
void pin_thread_to_cpu(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// ==============================================================================
// 1. SPNED 原生工作线程 (Zero-Contention Native Execution)
// ==============================================================================
void spned_worker(const uint32_t* __restrict__ in, uint32_t* __restrict__ out, const uint32_t* __restrict__ dict, size_t start, size_t end, int thread_id) {
    pin_thread_to_cpu(thread_id); // 硬件级绑核
    for (size_t i = start; i < end; i += MORSEL_SIZE) {
        size_t batch = std::min(MORSEL_SIZE, end - i);
        for (size_t j = 0; j < batch; ++j) {
            out[i + j] = dict[in[i + j]];
        }
    }
}

// ==============================================================================
// 2. Wasm / SFI 工作线程 (Sandbox with Synchronization & SFI)
// ==============================================================================
void wasm_worker(uint32_t in_offset, uint32_t out_offset, uint32_t dict_offset, size_t start, size_t end, uint8_t* wasm_memory, uint32_t mem_size, int thread_id) {
    pin_thread_to_cpu(thread_id); // 硬件级绑核
    for (size_t i = start; i < end; i += MORSEL_SIZE) {
        size_t batch = std::min(MORSEL_SIZE, end - i);
        
        // 模拟 Wasm 跨界状态同步 (引发缓存行颠簸)
        wasm_instance_ref_count.fetch_add(1, std::memory_order_acq_rel);
        
        for (size_t j = 0; j < batch; ++j) {
            size_t global_idx = i + j;
            uint32_t read_idx_addr = in_offset + global_idx * 4;
            if (__builtin_expect(read_idx_addr + 4 > mem_size, 0)) break;
            uint32_t idx = *reinterpret_cast<uint32_t*>(wasm_memory + read_idx_addr);

            uint32_t dict_addr = dict_offset + idx * 4;
            if (__builtin_expect(dict_addr + 4 > mem_size, 0)) break;
            uint32_t val = *reinterpret_cast<uint32_t*>(wasm_memory + dict_addr);

            uint32_t write_addr = out_offset + global_idx * 4;
            if (__builtin_expect(write_addr + 4 > mem_size, 0)) break;
            *reinterpret_cast<uint32_t*>(wasm_memory + write_addr) = val;
        }

        wasm_instance_ref_count.fetch_sub(1, std::memory_order_acq_rel);
    }
}

int main() {
    cout << "======================================================================\n";
    cout << "  Experiment 4: NUMA-Aware Scalability & Hardware Thread Pinning\n";
    cout << "======================================================================\n";

    vector<uint32_t> dict(DICT_SIZE);
    vector<uint32_t> input(TOTAL_TUPLES);
    vector<uint32_t> output(TOTAL_TUPLES);
    
    uint32_t mem_size = (DICT_SIZE + TOTAL_TUPLES * 2) * 4;
    vector<uint8_t> wasm_linear_memory(mem_size);
    uint32_t dict_offset = 0;
    uint32_t in_offset = DICT_SIZE * 4;
    uint32_t out_offset = (DICT_SIZE + TOTAL_TUPLES) * 4;

    mt19937 rng(42);
    for (size_t i = 0; i < DICT_SIZE; ++i) {
        uint32_t val = rng();
        dict[i] = val;
        reinterpret_cast<uint32_t*>(wasm_linear_memory.data() + dict_offset)[i] = val;
    }

    cout << "  [System] Executing Pinned First-Touch Memory Allocation...\n";
    
    // ==========================================================================
    // 强制绑核的 NUMA-Aware First-Touch
    // 让指定的物理核心去初始化其对应的内存区块，彻底锁死数据与计算单元的本地映射。
    // ==========================================================================
    int num_init_threads = 64; 
    vector<thread> init_threads;
    size_t chunk_size = TOTAL_TUPLES / num_init_threads;
    
    auto init_worker = [&](size_t start, size_t end, int thread_id) {
        pin_thread_to_cpu(thread_id); // 保证在哪颗核上计算，就在哪颗核的内存条上分配！
        mt19937 local_rng(42 + start);
        uniform_int_distribution<uint32_t> local_dist(0, DICT_SIZE - 1);
        for (size_t i = start; i < end; ++i) {
            uint32_t val = local_dist(local_rng);
            input[i] = val;
            output[i] = 0;
            reinterpret_cast<uint32_t*>(wasm_linear_memory.data() + in_offset)[i] = val;
            reinterpret_cast<uint32_t*>(wasm_linear_memory.data() + out_offset)[i] = 0;
        }
    };

    for (int i = 0; i < num_init_threads; ++i) {
        size_t start = i * chunk_size;
        size_t end = (i == num_init_threads - 1) ? TOTAL_TUPLES : start + chunk_size;
        init_threads.emplace_back(init_worker, start, end, i);
    }
    for (auto& th : init_threads) th.join();

    cout << "  [System] Thread affinity enforced. Data topology locked. Ready.\n";
    cout << "----------------------------------------------------------------------\n";
    
    vector<int> thread_counts = {1, 4, 8, 16, 32, 64};
    
    cout << left << setw(10) << "Threads" 
         << setw(30) << "SPNED Perf (MT/s | GB/s)" 
         << setw(30) << "Wasm Perf (MT/s | GB/s)" << "\n";
    cout << "----------------------------------------------------------------------\n";

    const double BYTES_PER_TUPLE = 12.0;

    for (int t : thread_counts) {
        // 测量 SPNED
        vector<thread> threads;
        auto start_spned = chrono::high_resolution_clock::now();
        size_t t_chunk = TOTAL_TUPLES / t;
        for (int i = 0; i < t; ++i) {
            size_t start = i * t_chunk;
            size_t end = (i == t - 1) ? TOTAL_TUPLES : start + t_chunk;
            threads.emplace_back(spned_worker, input.data(), output.data(), dict.data(), start, end, i); // 传入 thread_id 绑核
        }
        for (auto& th : threads) th.join();
        double spned_time = chrono::duration_cast<chrono::duration<double>>(chrono::high_resolution_clock::now() - start_spned).count();
        double spned_mts = (TOTAL_TUPLES / spned_time) / 1e6;
        double spned_gbs = (TOTAL_TUPLES * BYTES_PER_TUPLE / spned_time) / 1e9;

        // 测量 Wasm
        threads.clear();
        auto start_wasm = chrono::high_resolution_clock::now();
        for (int i = 0; i < t; ++i) {
            size_t start = i * t_chunk;
            size_t end = (i == t - 1) ? TOTAL_TUPLES : start + t_chunk;
            threads.emplace_back(wasm_worker, in_offset, out_offset, dict_offset, start, end, wasm_linear_memory.data(), mem_size, i); // 传入 thread_id 绑核
        }
        for (auto& th : threads) th.join();
        double wasm_time = chrono::duration_cast<chrono::duration<double>>(chrono::high_resolution_clock::now() - start_wasm).count();
        double wasm_mts = (TOTAL_TUPLES / wasm_time) / 1e6;
        double wasm_gbs = (TOTAL_TUPLES * BYTES_PER_TUPLE / wasm_time) / 1e9;

        char spned_str[64], wasm_str[64];
        snprintf(spned_str, sizeof(spned_str), "%.2f | %.2f", spned_mts, spned_gbs);
        snprintf(wasm_str, sizeof(wasm_str), "%.2f | %.2f", wasm_mts, wasm_gbs);

        cout << left << setw(10) << t 
             << setw(30) << spned_str 
             << setw(30) << wasm_str << "\n";
    }

    cout << "======================================================================\n";
    return 0;
}
