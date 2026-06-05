#include <cstdint>
#include <cstddef>
extern "C" {
    void spned_decode_seq(const uint32_t* runs, const uint32_t* dicts, uint32_t* out, size_t size) {
        size_t out_idx = 0;
        for (size_t i = 0; i < size; ++i) {
            uint32_t run = runs[i], val = dicts[i];
            for (uint32_t r = 0; r < run; ++r) out[out_idx++] = val;
        }
    }
    void spned_decode_sel(const uint32_t* runs, const uint32_t* dicts, uint32_t* out, size_t size) {
        size_t out_idx = 0;
        for (size_t i = 0; i < size; ++i) {
            uint32_t run = runs[i], val = dicts[i];
            if (val % 10 == 0) {
                for (uint32_t r = 0; r < run; ++r) out[out_idx++] = val;
            }
        }
    }
}
