#!/bin/bash
set -e
export SYS_PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
env PATH=$SYS_PATH g++ -O3 -std=c++17 measure_batch_overhead.cpp -o measure_batch_overhead
./measure_batch_overhead | tee experiment3_results.txt
