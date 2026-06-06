#!/bin/bash
echo "========================================================"
echo " SPNED Transparent Evaluation Engine (No Black-Boxes)"
echo "========================================================"

COMPILER=$(which g++)
if [ -f "/usr/bin/g++" ]; then COMPILER="/usr/bin/g++"; fi
echo "[+] Using compiler: $COMPILER"

echo -e "\n>>> Phase A: Generic Micro-JIT Compilation Engine"
$COMPILER -O3 -std=c++17 spned_transparent_core.cpp -o spned_transparent_core_bin
./spned_transparent_core_bin

echo -e "\n>>> Phase B: Authentic DAG Control Flow Graph (Worklist) Verifier"
$COMPILER -O3 exp2_cfg_worklist.cpp -o exp2_cfg_worklist_bin
./exp2_cfg_worklist_bin
