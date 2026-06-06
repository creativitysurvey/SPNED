#!/bin/bash
echo "========================================================"
echo " SPNED Final AE Verification Suite (True OS & AST Level)"
echo "========================================================"

echo -e "\n>>> 1. Compiling Exp 2: True AST Parser & Worklist Verifier..."
g++ -O3 -std=c++17 spned_real_ast_parser.cpp -o spned_real_ast_parser
./spned_real_ast_parser

echo -e "\n>>> 2. Compiling Exp 1: Authentic Wasm MMU & Linux eBPF Kernel Call..."
g++ -O3 -std=c++17 measure_exp1_kernel_real.cpp -o measure_exp1_kernel_real
./measure_exp1_kernel_real

# 之前修好的 exp3 和 exp4 依然保留
echo -e "\n>>> 3. Executing Exp 3 (Dynamic Freq Probe)..."
./exp3_dynamic_probe
echo -e "\n>>> 4. Executing Exp 4 (NUMA Scaling)..."
./exp4_bin
