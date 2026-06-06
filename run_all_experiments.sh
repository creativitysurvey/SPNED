#!/bin/bash
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (True Hardware Level)"
echo "========================================================"

echo -e "\n>>> 1. Compiling & Executing Exp 1: Physical Isolation Bound Analysis (mmap)..."
g++ -O3 -march=native -std=c++17 measure_exp1.cpp -o measure_exp1_bin
./measure_exp1_bin

echo -e "\n>>> 2. Compiling & Executing Exp 3: True Dynamic Pipeline Probe (rdtscp)..."
g++ -O3 -march=native exp3_dynamic_probe.cpp -o exp3_dynamic_probe
./exp3_dynamic_probe

echo -e "\n>>> 3. Compiling & Executing Exp 4: NUMA-Aware Concurrent Scalability (-pthread)..."
g++ -O3 -pthread exp4_numa_scaling.cpp -o exp4_bin
./exp4_bin

echo -e "\n>>> 4. Compiling & Executing Exp 2: Unified CFG Worklist & JIT Engine..."
g++ -O3 -std=c++17 spned_unified_engine.cpp -o spned_unified_engine
./spned_unified_engine
