typedef unsigned long long uint64_t;
struct bpf_ctx { int* arr; uint64_t count; };
uint64_t func(struct bpf_ctx *ctx) {
    int* arr = ctx->arr;
    uint64_t count = ctx->count;
    uint64_t sum = 0;
    for (uint64_t i = 0; i < 10000000; i++) {
        if (i >= count) break;
        if (arr[i] < 50) sum += arr[i];
    }
    return sum;
}
