#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>
#include <iomanip>
#include <wasm.h>
#include <wasmtime.h>

// SPNED 的物理优势：同一地址空间，无需跨沙盒拷贝数据，直接裸机执行
__attribute__((noinline)) void spned_worker(int* arr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        arr[i] = (arr[i] + 10) * 2;
    }
}

int main() {
    std::cout << "=== Exp 3: TRUE Wasmtime C API FFI Context Switch Latency ===\n";
    std::cout << "[+] Probing boundary crossing overhead across 8 payload sizes...\n\n";

    // 测试 8 个不同的 Payload 规模 (元素个数): 0, 16, 64, 256, 1024, 4096, 16384, 65536
    // 对应字节数(Bytes): 0 B, 64 B, 256 B, 1 KB, 4 KB, 16 KB, 64 KB, 256 KB
    std::vector<size_t> payload_sizes = {0, 16, 64, 256, 1024, 4096, 16384, 65536};
    
    // 初始化 Wasmtime
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* ctx = wasmtime_store_context(store);

    const char* wat = 
        "(module"
        "  (memory (export \"mem\") 1000)" // 分配 1000 页，足够容纳 256KB
        "  (func (export \"run\") (param $len i32)"
        "    (local $i i32) (local $ptr i32)"
        "    (loop $my_loop"
        "      (if (i32.lt_s (local.get $i) (local.get $len))"
        "        (then"
        "          (local.set $ptr (i32.mul (local.get $i) (i32.const 4)))"
        "          (i32.store (local.get $ptr) (i32.mul (i32.add (i32.load (local.get $ptr)) (i32.const 10)) (i32.const 2)))"
        "          (local.set $i (i32.add (local.get $i) (i32.const 1)))"
        "          (br $my_loop)"
        "        )))))";
        
    wasm_byte_vec_t wasm_bytes;
    wasmtime_wat2wasm(wat, strlen(wat), &wasm_bytes);
    wasmtime_module_t* module = nullptr;
    wasmtime_module_new(engine, (uint8_t*)wasm_bytes.data, wasm_bytes.size, &module);
    wasmtime_instance_t instance;
    wasmtime_instance_new(ctx, module, nullptr, 0, &instance, nullptr);
    
    wasmtime_extern_t run_extern, mem_extern;
    wasmtime_instance_export_get(ctx, &instance, "run", 3, &run_extern);
    wasmtime_instance_export_get(ctx, &instance, "mem", 3, &mem_extern);
    
    const wasmtime_func_t* wasm_func = &run_extern.of.func;
    uint8_t* wasm_memory = wasmtime_memory_data(ctx, &mem_extern.of.memory);

    std::cout << std::left << std::setw(20) << "Payload Size (B)" 
              << std::setw(25) << "SPNED Latency (ns/op)" 
              << "Wasm FFI Latency (ns/op)\n";
    std::cout << "----------------------------------------------------------------------\n";

    for (size_t size : payload_sizes) {
        size_t bytes = size * 4;
        std::vector<int> data(size == 0 ? 1 : size, 5); // 即使 size=0 也保证指针有效
        
        // 动态调整迭代次数：数据越小跑得越多次，保证计时器精度
        int ITERS = 500000;
        if (size > 1000) ITERS = 50000;
        if (size > 10000) ITERS = 5000;

        // --- 1. SPNED 原地执行测速 (Zero-copy) ---
        auto start_spned = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERS; ++i) {
            spned_worker(data.data(), size);
            asm volatile("" ::: "memory"); // 内存屏障，防止 GCC 死代码消除
        }
        auto end_spned = std::chrono::high_resolution_clock::now();
        double spned_ns = std::chrono::duration<double, std::nano>(end_spned - start_spned).count() / ITERS;

        // --- 2. Wasmtime 沙盒越界测速 (Copy + FFI + Copy) ---
        auto start_wasm = std::chrono::high_resolution_clock::now();
        wasmtime_val_t args[1], results[1];
        args[0].kind = WASMTIME_I32; args[0].of.i32 = size;
        
        for (int i = 0; i < ITERS; ++i) {
            // Wasm 的物理代价：必须将数据拷入线性内存
            if (size > 0) std::memcpy(wasm_memory, data.data(), bytes);
            
            // 真实触发虚拟机上下文陷入 (Context Switch)
            wasmtime_func_call(ctx, wasm_func, args, 1, results, 0, nullptr);
            
            // 将处理完的数据拷贝回宿主机
            if (size > 0) std::memcpy(data.data(), wasm_memory, bytes);
            
            asm volatile("" ::: "memory");
        }
        auto end_wasm = std::chrono::high_resolution_clock::now();
        double wasm_ns = std::chrono::duration<double, std::nano>(end_wasm - start_wasm).count() / ITERS;

        // 打印完美对齐的 16 个测量值
        std::string size_str = (bytes == 0) ? "0 B (Empty)" : std::to_string(bytes) + " B";
        std::cout << std::left << std::setw(20) << size_str 
                  << std::fixed << std::setprecision(2) << std::setw(25) << spned_ns
                  << std::fixed << std::setprecision(2) << wasm_ns << "\n";
    }
    std::cout << "----------------------------------------------------------------------\n";

    wasm_byte_vec_delete(&wasm_bytes);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}
