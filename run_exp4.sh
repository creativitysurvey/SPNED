#!/bin/bash
echo ">>> Compiling NUMA-Aware Concurrent Bandwidth Probe (-pthread)..."
g++ -O3 -pthread exp4_numa_scaling.cpp -o exp4_numa_scaling_bin
if [ $? -eq 0 ]; then
    ./exp4_numa_scaling_bin
else
    echo "[-] Compilation failed."
fi
