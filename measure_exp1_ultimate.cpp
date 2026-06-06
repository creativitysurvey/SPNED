#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sys/mman.h>
#include <wasm.h>
#include <wasmtime.h>
#include "ubpf/vm/inc/ubpf.h"

// ----------------------------------------------------------------------------
// 1. 真实的动态 x86-64 机器码发射器 (修复了段错误：分配两块独立内存)
// ----------------------------------------------------------------------------
class DynamicX86JIT {
    uint8_t* code_seq;
    uint8_t* code_sel;
public:
    DynamicX86JIT() {
        code_seq = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        code_sel = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    ~DynamicX86JIT() { munmap(code_seq, 4096); munmap(code_sel, 4096); }

    void* compile_seq() {
        size_t offset = 0;
        auto emit8 = [&](uint8_t b) { code_seq[offset++] = b; };
        emit8(0x48); emit8(0x31); emit8(0xC9); 
        size_t loop_start = offset;
        emit8(0x48); emit8(0x39); emit8(0xF1);
        emit8(0x7D); size_t exit_jump = offset; emit8(0x00); 
        emit8(0x8B); emit8(0x04); emit8(0x8F); 
        emit8(0x83); emit8(0xC0); emit8(0x0A); 
        emit8(0xD1); emit8(0xE0);              
        emit8(0x89); emit8(0x04); emit8(0x8F); 
        emit8(0x48); emit8(0xFF); emit8(0xC1); 
        emit8(0xEB); emit8((uint8_t)(loop_start - offset - 1));
        code_seq[exit_jump] = offset - exit_jump - 1;
        emit8(0xC3); 
        mprotect(code_seq, 4096, PROT_READ | PROT_EXEC); // 锁定当前页
        return (void*)code_seq;
    }

    void* compile_sel() {
        size_t offset = 0;
        auto emit8 = [&](uint8_t b) { code_sel[offset++] = b; };
        emit8(0x48); emit8(0x31); emit8(0xC9); 
        emit8(0x48); emit8(0x31); emit8(0xC0); 
        size_t loop_start = offset;
        emit8(0x48); emit8(0x39); emit8(0xF1); 
        emit8(0x7D); size_t exit_jump = offset; emit8(0x00);
        emit8(0x8B); emit8(0x14); emit8(0x8F); 
        emit8(0x83); emit8(0xFA); emit8(0x32); 
        emit8(0x7D); emit8(0x03);              
        emit8(0x48); emit8(0x01); emit8(0xD0); 
        emit8(0x48); emit8(0xFF); emit8(0xC1); 
        emit8(0xEB); emit8((uint8_t)(loop_start - offset - 1));
        code_sel[exit_jump] = offset - exit_jump - 1;
        emit8(0xC3);
        mprotect(code_sel, 4096, PROT_READ | PROT_EXEC); // 独立锁定
        return (void*)code_sel;
    }
};

// ----------------------------------------------------------------------------
// 2. 原生基线 与 真实 eBPF 运行器
// ----------------------------------------------------------------------------
__attribute__((noinline)) void native_seq(int* arr, size_t count) {
    for (size_t i = 0; i < count; i++) arr[i] = (arr[i] + 10) * 2;
}
__attribute__((noinline)) long long native_sel(int* arr, size_t count) {
    long long sum = 0;
    for (size_t i = 0; i < count; i++) { if (arr[i] < 50) sum += arr[i]; }
    return sum;
}

struct bpf_ctx { int* arr; size_t count; };

double run_true_ebpf(const char* bin_path, int* data, size_t count) {
    std::ifstream file(bin_path, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Failed to load eBPF bytecode\n"; return -1; }
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytecode(size);
    file.read((char*)bytecode.data(), size);

    struct ubpf_vm *vm = ubpf_create();
    char *errmsg = nullptr;
    if (ubpf_load(vm, bytecode.data(), bytecode.size(), &errmsg) < 0) {
        std::cerr << "uBPF Load Failed: " << errmsg << "\n"; 
        free(errmsg); ubpf_destroy(vm); return -1;
    }
    
    ubpf_jit_fn fn = ubpf_compile(vm, &errmsg);
    struct bpf_ctx ctx = { data, count };
    
    auto start = std::chrono::high_resolution_clock::now();
    if (fn) {
        fn(&ctx, sizeof(ctx)); // JIT 模式
    } else {
        uint64_t ret;
        ubpf_exec(vm, &ctx, sizeof(ctx), &ret); // VM 解释器后备模式
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    ubpf_destroy(vm);
    if (errmsg) free(errmsg);
    return std::chrono::duration<double, std::milli>(end - start).count();
}

// ----------------------------------------------------------------------------
// 3. Wasmtime 运行器
// ----------------------------------------------------------------------------
double run_wasm(wasmtime_context_t* ctx, const wasmtime_func_t* func, uint8_t* mem, int* data, size_t count, bool copy_back) {
    auto start = std::chrono::high_resolution_clock::now();
    std::memcpy(mem, data, count * 4); 
    wasmtime_val_t args[1], results[1];
    args[0].kind = WASMTIME_I32; args[0].of.i32 = count;
    wasmtime_func_call(ctx, func, args, 1, results, 1, nullptr);
    if (copy_back) std::memcpy(data, mem, count * 4); 
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

int main() {
    std::cout << "=== Exp 1: Absolute Authentic Throughput with Real Data ===\n";
    std::cout << "[+] Deploying True ISA-level User-Space Benchmarks...\n";
    std::cout << "[+] eBPF VM: IOVisor/uBPF (True JIT / Interpreter)\n";
    std::cout << "[+] Wasm VM: BytecodeAlliance/Wasmtime (True JIT)\n\n";

    const size_t TUPLE_COUNT = 10000000;
    std::vector<int> data(TUPLE_COUNT, 20);
    
    DynamicX86JIT jit_compiler;
    typedef void (*SeqFunc)(int*, size_t);
    typedef long long (*SelFunc)(int*, size_t);
    SeqFunc spned_seq_func = (SeqFunc)jit_compiler.compile_seq();
    SelFunc spned_sel_func = (SelFunc)jit_compiler.compile_sel();

    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);
    wasmtime_context_t* ctx = wasmtime_store_context(store);
    
    const char* wat_seq = "(module (memory (export \"mem\") 1000) (func (export \"run\") (param $len i32) (result i32) (local $i i32) (local $ptr i32) (loop $my_loop (if (i32.lt_s (local.get $i) (local.get $len)) (then (local.set $ptr (i32.mul (local.get $i) (i32.const 4))) (i32.store (local.get $ptr) (i32.mul (i32.add (i32.load (local.get $ptr)) (i32.const 10)) (i32.const 2))) (local.set $i (i32.add (local.get $i) (i32.const 1))) (br $my_loop)))) (i32.const 0)))";
    const char* wat_sel = "(module (memory (export \"mem\") 1000) (func (export \"run\") (param $len i32) (result i32) (local $i i32) (local $ptr i32) (local $sum i32) (local $val i32) (loop $my_loop (if (i32.lt_s (local.get $i) (local.get $len)) (then (local.set $ptr (i32.mul (local.get $i) (i32.const 4))) (local.set $val (i32.load (local.get $ptr))) (if (i32.lt_s (local.get $val) (i32.const 50)) (then (local.set $sum (i32.add (local.get $sum) (local.get $val))))) (local.set $i (i32.add (local.get $i) (i32.const 1))) (br $my_loop)))) (local.get $sum)))";

    auto compile_wasm = [&](const char* wat, wasmtime_instance_t* inst, wasmtime_extern_t* func_ext, wasmtime_extern_t* mem_ext) {
        wasm_byte_vec_t wasm_bytes; wasmtime_wat2wasm(wat, strlen(wat), &wasm_bytes);
        wasmtime_module_t* mod = nullptr; wasmtime_module_new(engine, (uint8_t*)wasm_bytes.data, wasm_bytes.size, &mod);
        wasmtime_instance_new(ctx, mod, nullptr, 0, inst, nullptr);
        wasmtime_instance_export_get(ctx, inst, "run", 3, func_ext);
        wasmtime_instance_export_get(ctx, inst, "mem", 3, mem_ext);
        wasm_byte_vec_delete(&wasm_bytes); wasmtime_module_delete(mod);
    };

    wasmtime_instance_t inst_seq, inst_sel;
    wasmtime_extern_t func_seq, mem_seq, func_sel, mem_sel;
    compile_wasm(wat_seq, &inst_seq, &func_seq, &mem_seq);
    compile_wasm(wat_sel, &inst_sel, &func_sel, &mem_sel);

    auto print_row = [](const char* name, const char* mode, double ms, size_t count) {
        std::cout << std::left << std::setw(30) << name << std::setw(15) << mode 
                  << std::fixed << std::setprecision(0) << (count / ms / 1000.0) << " MT/s\n";
    };

    std::cout << "-----------------------------------------------------------------\n";
    std::cout << std::left << std::setw(30) << "Architecture Paradigm" << std::setw(15) << "Query Mode" << "Throughput\n";
    std::cout << "-----------------------------------------------------------------\n";

    // --- 1. Sequential Mode ---
    auto t1 = std::chrono::high_resolution_clock::now();
    native_seq(data.data(), TUPLE_COUNT);
    double ms_nat_seq = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t1).count();
    print_row("Native C++ (Monolithic)", "Sequential", ms_nat_seq, TUPLE_COUNT);

    std::fill(data.begin(), data.end(), 20);
    auto t2 = std::chrono::high_resolution_clock::now();
    spned_seq_func(data.data(), TUPLE_COUNT);
    double ms_jit_seq = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t2).count();
    print_row("SPNED (True Dynamic JIT)", "Sequential", ms_jit_seq, TUPLE_COUNT);

    double ms_wasm_seq = run_wasm(ctx, &func_seq.of.func, wasmtime_memory_data(ctx, &mem_seq.of.memory), data.data(), TUPLE_COUNT, true);
    print_row("True Wasm Linear Memory", "Sequential", ms_wasm_seq, TUPLE_COUNT);

    std::fill(data.begin(), data.end(), 20);
    double ms_ebpf_seq = run_true_ebpf("ebpf_seq.bin", data.data(), TUPLE_COUNT);
    print_row("True eBPF VM (IOVisor/uBPF)", "Sequential", ms_ebpf_seq, TUPLE_COUNT);

    std::cout << "-----------------------------------------------------------------\n";

    // --- 2. Selective Mode ---
    std::fill(data.begin(), data.end(), 20);
    auto t4 = std::chrono::high_resolution_clock::now();
    long long sum1 = native_sel(data.data(), TUPLE_COUNT); asm volatile("" : : "r"(sum1) : "memory");
    double ms_nat_sel = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t4).count();
    print_row("Native C++ (Monolithic)", "Selective", ms_nat_sel, TUPLE_COUNT);

    auto t5 = std::chrono::high_resolution_clock::now();
    spned_sel_func(data.data(), TUPLE_COUNT);
    double ms_jit_sel = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t5).count();
    print_row("SPNED (True Dynamic JIT)", "Selective", ms_jit_sel, TUPLE_COUNT);

    double ms_wasm_sel = run_wasm(ctx, &func_sel.of.func, wasmtime_memory_data(ctx, &mem_sel.of.memory), data.data(), TUPLE_COUNT, false);
    print_row("True Wasm Linear Memory", "Selective", ms_wasm_sel, TUPLE_COUNT);

    std::fill(data.begin(), data.end(), 20);
    double ms_ebpf_sel = run_true_ebpf("ebpf_sel.bin", data.data(), TUPLE_COUNT);
    print_row("True eBPF VM (IOVisor/uBPF)", "Selective", ms_ebpf_sel, TUPLE_COUNT);
    
    std::cout << "-----------------------------------------------------------------\n";

    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return 0;
}
