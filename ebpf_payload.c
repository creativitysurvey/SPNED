// 彻底抛弃 #include <stdint.h> 和 <stddef.h>
// 直接手动定义 64 位无符号整数，彻底斩断与 Conda 系统头文件的联系！
typedef unsigned long long uint64_t;
typedef unsigned long long size_t;

struct bpf_ctx {
    int* arr;
    size_t count;
};

// 显式声明独立 section，保证 objcopy 能完美提取纯净的物理机器指令
__attribute__((section(".text.bpf_seq")))
uint64_t bpf_seq(struct bpf_ctx *ctx) {
    int* arr = ctx->arr;
    size_t count = ctx->count;
    for (size_t i = 0; i < count; i++) {
        if (i >= 10000000) break; // eBPF 的绝对安全底线
        arr[i] = (arr[i] + 10) * 2;
    }
    return 0;
}

__attribute__((section(".text.bpf_sel")))
uint64_t bpf_sel(struct bpf_ctx *ctx) {
    int* arr = ctx->arr;
    size_t count = ctx->count;
    long long sum = 0;
    for (size_t i = 0; i < count; i++) {
        if (i >= 10000000) break;
        if (arr[i] < 50) sum += arr[i];
    }
    return sum;
}
