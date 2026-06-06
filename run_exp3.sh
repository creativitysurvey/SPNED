#!/bin/bash
echo ">>> Compiling Hardware Pipeline Serialization Probe (rdtscp)..."
g++ -O3 -march=native exp3_rdtsc_probe.cpp -o exp3_rdtsc_probe_bin
if [ $? -eq 0 ]; then
    ./exp3_rdtsc_probe_bin
else
    echo "[-] Compilation failed."
fi
