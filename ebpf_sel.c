typedef unsigned int uint32_t; typedef unsigned long long uint64_t;
struct decode_ctx { const uint32_t* runs; const uint32_t* dicts; uint32_t* out; uint32_t size; uint32_t max_out; };
__attribute__((section(".text")))
uint64_t decode_sel(struct decode_ctx *ctx) {
    uint32_t out_idx = 0, size = ctx->size, max_out = ctx->max_out;
    for (uint32_t i = 0; i < 10000000; ++i) {
        if (i >= size) break;
        uint32_t run = ctx->runs[i], val = ctx->dicts[i];
        if (val % 10 == 0) {
            for (uint32_t r = 0; r < 50; ++r) {
                if (r >= run) break;
                if (out_idx >= max_out) return out_idx;
                ctx->out[out_idx++] = val;
            }
        }
    }
    return out_idx;
}
