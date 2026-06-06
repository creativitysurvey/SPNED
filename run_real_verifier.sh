#!/bin/bash
echo "========================================================"
echo " SPNED Abstract Interpreter AE Execution Script"
echo "========================================================"

# 强制使用系统原生的 g++，避免 conda/brew 等环境的 linker 污染
COMPILER=$(which g++)
if [ -f "/usr/bin/g++" ]; then
    COMPILER="/usr/bin/g++"
fi

echo "[+] Using compiler: $COMPILER"
$COMPILER -O3 -std=c++17 spned_real_verifier.cpp -o spned_real_verifier_bin

if [ $? -eq 0 ]; then
    echo "[+] Compilation successful. Executing..."
    echo ""
    ./spned_real_verifier_bin
else
    echo "[-] Compilation failed. Please ensure GCC 9+ is installed."
fi
