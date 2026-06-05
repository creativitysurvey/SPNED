typedef unsigned int uint32_t;
uint32_t run_lengths[1000000]; uint32_t dict_ids[1000000]; uint32_t output_buffer[10000000];
__attribute__((export_name("get_runs_ptr"))) uint32_t* get_runs_ptr() { return run_lengths; }
__attribute__((export_name("get_dicts_ptr"))) uint32_t* get_dicts_ptr() { return dict_ids; }
__attribute__((export_name("get_out_ptr"))) uint32_t* get_out_ptr() { return output_buffer; }

__attribute__((export_name("decode_seq")))
void decode_seq(uint32_t size) {
    uint32_t out_idx = 0;
    for (uint32_t i = 0; i < size; ++i) {
        uint32_t run = run_lengths[i], val = dict_ids[i];
        for (uint32_t r = 0; r < run; ++r) output_buffer[out_idx++] = val;
    }
}
__attribute__((export_name("decode_sel")))
void decode_sel(uint32_t size) {
    uint32_t out_idx = 0;
    for (uint32_t i = 0; i < size; ++i) {
        uint32_t run = run_lengths[i], val = dict_ids[i];
        if (val % 10 == 0) {
            for (uint32_t r = 0; r < run; ++r) output_buffer[out_idx++] = val;
        }
    }
}
