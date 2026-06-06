#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <sys/mman.h>
#include <wasm.h>
#include <wasmtime.h>

class RealJitEngine {
    uint8_t* code;
public:
    RealJitEngine() {
        code = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        uint8_t bytes[] = {
            0x48, 0x31, 0xC9,                   // xor rcx, rcx          ; i = 0
            // .loop:
            0x48, 0x39, 0xF1,                   // cmp rcx, rsi          ; i < count?
            0x7D, 0x10,                         // jge .done             ; [修复处!] 严格向前跳16个字节 (0x10)
            0x8B, 0x04, 0x8F,                   // mov eax, [rdi+rcx*4]  ; load arr[i]
            0x83, 0xC0, 0x0A,                   // add eax, 10           ; + 10
            0xD1, 0xE0,                         // shl eax, 1            ; * 2
            0x89, 0x04, 0x8F,                   // mov [rdi+rcx*4], eax  ; store arr[i]
            0x48, 0xFF, 0xC1,                   // inc rcx               ; i++
            0xEB, 0xEB,                         // jmp .loop             ; jump back 
            // .done:
            0xC3                                // ret
        };
        std::memcpy(code, bytes, sizeof(bytes));
        mprotect(code, 4096, PROT_READ | PROT_EXEC);
    }
    ~RealJitEngine() { munmap(code, 4096); }
    // [修复处!] 强制使用 size_t 保证 64 位 rsi 寄存器高位无垃圾数据
    typedef void (*DecodeFunc)(int*, size_t);
    DecodeFunc get_func() { return (DecodeFunc)code; }
};

__attribute__((noinline)) void decode_native(int* arr, size_t count) {
    for (size_t i = 0; i < count; i++) arr[i] = (arr[i] + 10) * 2;
}

int main() {
    std::cout << "=== Exp 1: TRUE Physical Throughput Benchmark ===\n";
    const size_t TUPLE_COUNT = 10000000;
    std::vector<int> data(TUPLE_COUNT, 5);

    auto t1 = std::chrono::high_resolution_clock::now();
    decode_native(data.data(), TUPLE_COUNT);
    asm volatile("" : : "r"(data[0]) : "memory");
    auto t2 = std::chrono::high_resolution_clock::now();
    double native_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

    std::fill(data.begin(), data.end(), 5);
    RealJitEngine jit;
    auto spned_func = jit.get_func();
    
    auto t3 = std::chrono::high_resolution_clock::now();
    spned_func(data.data(), TUPLE_COUNT);
    asm volatile("" : : "r"(data[0]) : "memory");
    auto t4 = std::chrono::high_resolution_clock::now();
    double jit_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();

    std::fill(data.begin(), data.end(), 5);
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* context = wasmtime_store_context(store);
    
    const char* wat = 
        "(module"
        "  (memory (export \"mem\") 1000)" 
        "  (func (export \"decode\") (param $len i32)"
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
    wasmtime_instance_new(context, module, nullptr, 0, &instance, nullptr);
    
    wasmtime_extern_t run_extern, mem_extern;
    wasmtime_instance_export_get(context, &instance, "decode", 6, &run_extern);
    wasmtime_instance_export_get(context, &instance, "mem", 3, &mem_extern);
    
    const wasmtime_func_t* wasm_func = &run_extern.of.func;
    uint8_t* wasm_memory = wasmtime_memory_data(context, &mem_extern.of.memory);
    
    auto t5 = std::chrono::high_resolution_clock::now();
    std::memcpy(wasm_memory, data.data(), TUPLE_COUNT * 4);
    
    wasmtime_val_t args[1], results[1];
    args[0].kind = WASMTIME_I32; args[0].of.i32 = TUPLE_COUNT;
    wasmtime_func_call(context, wasm_func, args, 1, results, 0, nullptr);
    
    std::memcpy(data.data(), wasm_memory, TUPLE_COUNT * 4);
    auto t6 = std::chrono::high_resolution_clock::now();
    double wasm_ms = std::chrono::duration<double, std::milli>(t6 - t5).count();

    std::cout << "---------------------------------------------------------\n";
    std::cout << "Architecture       | Latency (ms) | Throughput (MT/s)\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "Native C++         | " << native_ms << " ms      | " << (TUPLE_COUNT / native_ms / 1000.0) << " MT/s\n";
    std::cout << "SPNED (Real JIT)   | " << jit_ms << " ms      | " << (TUPLE_COUNT / jit_ms / 1000.0) << " MT/s\n";
    std::cout << "True Wasm Sandbox  | " << wasm_ms << " ms      | " << (TUPLE_COUNT / wasm_ms / 1000.0) << " MT/s\n";
    std::cout << "---------------------------------------------------------\n";

    wasm_byte_vec_delete(&wasm_bytes);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}
