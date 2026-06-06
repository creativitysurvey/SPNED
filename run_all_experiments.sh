#!/bin/bash
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH; unset LIBRARY_PATH

echo "========================================================"
echo " SPNED Final AE Verification Suite (True Hardware Level)"
echo "========================================================"

echo -e "\n>>> 1. Compiling & Executing True Dynamic Pipeline Probe (Exp 3)..."
g++ -O3 -march=native exp3_dynamic_probe.cpp -o exp3_dynamic_probe
./exp3_dynamic_probe

echo -e "\n>>> 2. Compiling & Executing Unified CFG Worklist & JIT Engine (Exp 2)..."
g++ -O3 -std=c++17 spned_unified_engine.cpp -o spned_unified_engine
./spned_unified_engine
