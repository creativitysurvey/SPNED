#include <benchmark/benchmark.h>
#include <wasmtime.h>
#include <dlfcn.h>
#include "ubpf.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>

struct REE_Dict_Batch { std::vector<uint32_t> run_lengths; std::vector<uint32_t> dict_ids; };
const size_t NUM_TUPLES = 10000000;
REE_Dict_Batch global_data;
std::vector<uint32_t> native_output_buffer(NUM_TUPLES);

typedef void (*spned_func_t)(const uint32_t*, const uint32_t*, uint32_t*, size_t);
spned_func_t spned_seq_ptr = nullptr, spned_sel_ptr = nullptr;
wasm_engine_t* engine; wasmtime_store_t* store; wasmtime_context_t* context; wasmtime_instance_t instance;
uint8_t* wasm_memory; wasmtime_func_t wasm_seq_func, wasm_sel_func; uint32_t wasm_runs_off, wasm_dicts_off;
ubpf_jit_fn ebpf_jit_seq = nullptr, ebpf_jit_sel = nullptr;
struct decode_ctx { const uint32_t* runs; const uint32_t* dicts; uint32_t* out; uint32_t size; uint32_t max_out; };

void generate_test_data() {
    size_t current_tuples = 0;
    while (current_tuples < NUM_TUPLES) {
        uint32_t run_len = (std::rand() % 50) + 1;
        if (current_tuples + run_len > NUM_TUPLES) run_len = NUM_TUPLES - current_tuples;
        global_data.run_lengths.push_back(run_len); global_data.dict_ids.push_back(std::rand() % 100);
        current_tuples += run_len;
    }
}

void load_ubpf(struct ubpf_vm* vm, const char* filename, ubpf_jit_fn* jit_fn) {
    char* errmsg = nullptr;
    std::string path = std::string("../") + filename;
    std::ifstream file(path, std::ios::binary);
    if (!file) std::abort();
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    if (ubpf_load(vm, bytes.data(), bytes.size(), &errmsg) < 0 || !(*jit_fn = ubpf_compile(vm, &errmsg))) std::abort();
}

void init_engines() {
    // 1. SPNED Init
    void* handle = dlopen("../libspned.so", RTLD_NOW);
    spned_seq_ptr = (spned_func_t)dlsym(handle, "spned_decode_seq");
    spned_sel_ptr = (spned_func_t)dlsym(handle, "spned_decode_sel");

    // 2. Wasmtime Init
    wasm_config_t* config = wasm_config_new();
    wasmtime_config_cranelift_opt_level_set(config, WASMTIME_OPT_LEVEL_SPEED);
    engine = wasm_engine_new_with_config(config); store = wasmtime_store_new(engine, NULL, NULL); context = wasmtime_store_context(store);
    std::ifstream wasm_file("../decoder.wasm", std::ios::binary); std::vector<uint8_t> wasm_bytes((std::istreambuf_iterator<char>(wasm_file)), {});
    wasmtime_module_t* module = NULL; wasmtime_module_new(engine, wasm_bytes.data(), wasm_bytes.size(), &module);
    wasm_trap_t* trap = NULL; wasmtime_instance_new(context, module, NULL, 0, &instance, &trap);
    wasmtime_extern_t ext; wasmtime_instance_export_get(context, &instance, "memory", 6, &ext); wasm_memory = wasmtime_memory_data(context, &ext.of.memory);
    wasmtime_val_t res[1];
    wasmtime_instance_export_get(context, &instance, "get_runs_ptr", 12, &ext); wasmtime_func_call(context, &ext.of.func, NULL, 0, res, 1, &trap); wasm_runs_off = res[0].of.i32;
    wasmtime_instance_export_get(context, &instance, "get_dicts_ptr", 13, &ext); wasmtime_func_call(context, &ext.of.func, NULL, 0, res, 1, &trap); wasm_dicts_off = res[0].of.i32;
    wasmtime_instance_export_get(context, &instance, "decode_seq", 10, &ext); wasm_seq_func = ext.of.func;
    wasmtime_instance_export_get(context, &instance, "decode_sel", 10, &ext); wasm_sel_func = ext.of.func;

    // 3. eBPF Init
    struct ubpf_vm *vm_seq = ubpf_create(); load_ubpf(vm_seq, "ebpf_seq.bin", &ebpf_jit_seq);
    struct ubpf_vm *vm_sel = ubpf_create(); load_ubpf(vm_sel, "ebpf_sel.bin", &ebpf_jit_sel);
}

// ------------------------- BENCHMARKS -------------------------

// 1. Native
static void BM_Native_Seq(benchmark::State& state) {
    for (auto _ : state) {
        size_t out_idx = 0; size_t size = global_data.run_lengths.size();
        const uint32_t* runs = global_data.run_lengths.data();
        const uint32_t* dicts = global_data.dict_ids.data();
        uint32_t* out = native_output_buffer.data();
        for (size_t i = 0; i < size; ++i) {
            uint32_t run = runs[i]; uint32_t val = dicts[i];
            for (uint32_t r = 0; r < run; ++r) out[out_idx++] = val;
        }
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_Native_Seq);

static void BM_Native_Sel(benchmark::State& state) {
    for (auto _ : state) {
        size_t out_idx = 0; size_t size = global_data.run_lengths.size();
        const uint32_t* runs = global_data.run_lengths.data();
        const uint32_t* dicts = global_data.dict_ids.data();
        uint32_t* out = native_output_buffer.data();
        for (size_t i = 0; i < size; ++i) {
            uint32_t run = runs[i]; uint32_t val = dicts[i];
            if (val % 10 == 0) { for (uint32_t r = 0; r < run; ++r) out[out_idx++] = val; }
        }
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_Native_Sel);

// 2. SPNED
static void BM_SPNED_Seq(benchmark::State& state) {
    for (auto _ : state) {
        spned_seq_ptr(global_data.run_lengths.data(), global_data.dict_ids.data(), native_output_buffer.data(), global_data.run_lengths.size());
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_SPNED_Seq);

static void BM_SPNED_Sel(benchmark::State& state) {
    for (auto _ : state) {
        spned_sel_ptr(global_data.run_lengths.data(), global_data.dict_ids.data(), native_output_buffer.data(), global_data.run_lengths.size());
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_SPNED_Sel);

// 3. eBPF
static void BM_eBPF_Seq(benchmark::State& state) {
    decode_ctx ctx = {global_data.run_lengths.data(), global_data.dict_ids.data(), native_output_buffer.data(), (uint32_t)global_data.run_lengths.size(), NUM_TUPLES};
    for (auto _ : state) {
        ebpf_jit_seq(&ctx, sizeof(ctx));
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_eBPF_Seq);

static void BM_eBPF_Sel(benchmark::State& state) {
    decode_ctx ctx = {global_data.run_lengths.data(), global_data.dict_ids.data(), native_output_buffer.data(), (uint32_t)global_data.run_lengths.size(), NUM_TUPLES};
    for (auto _ : state) {
        ebpf_jit_sel(&ctx, sizeof(ctx));
        benchmark::DoNotOptimize(native_output_buffer); benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_eBPF_Sel);

// 4. Wasm Sandbox
static void BM_Wasm_Seq(benchmark::State& state) {
    size_t size = global_data.run_lengths.size();
    memcpy(wasm_memory + wasm_runs_off, global_data.run_lengths.data(), size * sizeof(uint32_t));
    memcpy(wasm_memory + wasm_dicts_off, global_data.dict_ids.data(), size * sizeof(uint32_t));
    for (auto _ : state) {
        wasmtime_val_t args[1]; args[0].kind = WASMTIME_I32; args[0].of.i32 = size; wasm_trap_t* trap = NULL;
        wasmtime_func_call(context, &wasm_seq_func, args, 1, NULL, 0, &trap);
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_Wasm_Seq);

static void BM_Wasm_Sel(benchmark::State& state) {
    size_t size = global_data.run_lengths.size();
    memcpy(wasm_memory + wasm_runs_off, global_data.run_lengths.data(), size * sizeof(uint32_t));
    memcpy(wasm_memory + wasm_dicts_off, global_data.dict_ids.data(), size * sizeof(uint32_t));
    for (auto _ : state) {
        wasmtime_val_t args[1]; args[0].kind = WASMTIME_I32; args[0].of.i32 = size; wasm_trap_t* trap = NULL;
        wasmtime_func_call(context, &wasm_sel_func, args, 1, NULL, 0, &trap);
    }
    state.SetItemsProcessed(state.iterations() * NUM_TUPLES);
}
BENCHMARK(BM_Wasm_Sel);

int main(int argc, char** argv) {
    generate_test_data(); init_engines();
    ::benchmark::Initialize(&argc, argv); ::benchmark::RunSpecifiedBenchmarks(); ::benchmark::Shutdown();
    return 0;
}
