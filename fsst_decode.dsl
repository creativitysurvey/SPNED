// FSST Decompression Real Logic
BB_START 0
LOAD_VAL idx 0
LOAD_VAL offset 0
BB_END 0 1

BB_START 1
// Loop Header
CMP idx 1000
BRANCH_IF_GREATER 4
BB_END 1 2

BB_START 2
// Indirect Dictionary Lookup (State Explosion Point)
LOAD_INDIRECT dict_val dict_base idx
ADD offset dict_val
STORE_SAFE out_buffer offset
ADD idx 1
BB_END 2 1 3

BB_START 3
// Error Fallback
RET -1
BB_END 3

BB_START 4
// Success
RET 0
BB_END 4
