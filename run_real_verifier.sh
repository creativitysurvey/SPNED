#!/bin/bash
echo "========================================================"
echo " SPNED Abstract Interpreter AE Execution Script"
echo "========================================================"

# 【核心修复】：在脚本内部临时剥离所有 Conda/Anaconda 相关的环境变量
# 这不会影响您外部的终端，仅对当前脚本的运行生效
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH
unset LIBRARY_PATH

COMPILER=$(which g++)
if [ -f "/usr/bin/g++" ]; then
    COMPILER="/usr/bin/g++"
fi

echo "[+] Using compiler: $COMPILER"
echo "[+] Environment sanitized (Conda linker bypassed)"

$COMPILER -O3 -std=c++17 spned_real_verifier.cpp -o spned_real_verifier_bin

if [ $? -eq 0 ]; then
    echo "[+] Compilation successful. Executing..."
    echo ""
    ./spned_real_verifier_bin
else
    echo "[-] Compilation failed. Please check system GCC."
fi
