#!/bin/bash
echo "========================================================"
echo " SPNED Transparent Evaluation Engine (No Black-Boxes)"
echo "========================================================"

# 【环境免疫】：剥离 Conda linker 干扰
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH
unset LIBRARY_PATH

COMPILER=$(which g++)
if [ -f "/usr/bin/g++" ]; then COMPILER="/usr/bin/g++"; fi

echo "[+] Using compiler: $COMPILER"
$COMPILER -O3 -std=c++17 spned_transparent_core.cpp -o spned_transparent_core_bin

if [ $? -eq 0 ]; then
    echo "[+] Compilation successful. Executing Transparent Engine..."
    echo ""
    ./spned_transparent_core_bin
else
    echo "[-] Compilation failed."
fi
