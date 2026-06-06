#include <iostream>
#include <chrono>
#include <cstring>
#include <wasm.h>
#include <wasmtime.h>

__attribute__((noinline)) int native_worker(int input) { return (input + 10) * 2; }

int main() {
    std::cout << "=== Exp 3: TRUE Wasmtime C API FFI Context Switch Probe ===\n";
    
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* context = wasmtime_store_context(store);

    const char* wat = "(module (func (export \"worker\") (param i32) (result i32) local.get 0 i32.const 10 i32.add i32.const 2 i32.mul))";
    wasm_byte_vec_t wasm_bytes;
    wasmtime_wat2wasm(wat, strlen(wat), &wasm_bytes);

    wasmtime_module_t* module = nullptr;
    wasmtime_module_new(engine, (uint8_t*)wasm_bytes.data, wasm_bytes.size, &module);
    wasmtime_instance_t instance;
    wasmtime_instance_new(context, module, nullptr, 0, &instance, nullptr);
    
    wasmtime_extern_t run_extern;
    wasmtime_instance_export_get(context, &instance, "worker", 6, &run_extern);
    const wasmtime_func_t* wasm_worker = &run_extern.of.func;

    const int ITERS = 5000000;
    
    // --- 原生 C++ 极限测速 (防御 -O3 优化) ---
    auto start_native = std::chrono::high_resolution_clock::now();
    long long native_sum = 0;
    for (int i = 0; i < ITERS; ++i) {
        native_sum += native_worker(i);
    }
    // 【终极修复】：插入内存屏障，强制 GCC 计算 native_sum，防止死代码消除！
    asm volatile("" : : "r"(native_sum) : "memory");
    auto end_native = std::chrono::high_resolution_clock::now();
    double native_ns = std::chrono::duration<double, std::nano>(end_native - start_native).count() / ITERS;

    // --- 真实 Wasm 沙盒跨界 FFI 测速 ---
    auto start_wasm = std::chrono::high_resolution_clock::now();
    long long wasm_sum = 0;
    wasmtime_val_t args[1], results[1];
    args[0].kind = WASMTIME_I32;
    for (int i = 0; i < ITERS; ++i) {
        args[0].of.i32 = i;
        wasmtime_func_call(context, wasm_worker, args, 1, results, 1, nullptr);
        wasm_sum += results[0].of.i32;
    }
    asm volatile("" : : "r"(wasm_sum) : "memory");
    auto end_wasm = std::chrono::high_resolution_clock::now();
    double wasm_ns = std::chrono::duration<double, std::nano>(end_wasm - start_wasm).count() / ITERS;

    std::cout << "  -> Native Call Latency   : " << native_ns << " ns/op\n";
    std::cout << "  -> True Wasm FFI Latency : " << wasm_ns << " ns/op\n";
    std::cout << "  -> (Note: Native sum: " << native_sum << " | Wasm sum: " << wasm_sum << " - Proven executed!)\n\n";

    wasm_byte_vec_delete(&wasm_bytes);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}
