#!/bin/bash
set -e

GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${GREEN}======================================================================${NC}"
echo -e "${GREEN}   Experiment 4: NUMA-Aware Multi-Core Scalability Profiler           ${NC}"
echo -e "${GREEN}======================================================================${NC}"
echo ""

echo -e "${CYAN}[1/2] Compiling concurrent stress test (-O3, -pthread)...${NC}"
export SYS_PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
env PATH=$SYS_PATH g++ -O3 -std=c++17 -pthread measure_scalability.cpp -o measure_scalability
echo "      -> Compilation successful."

echo -e "${CYAN}[2/2] Running 500 Million Tuple Stress Test...${NC}"
echo -e "${CYAN}      -> Pushing Dual-Socket DRAM to maximum bandwidth. Please wait...${NC}"
echo ""

./measure_scalability | tee experiment4_results.txt

echo ""
echo -e "${GREEN}SUCCESS: Results saved to experiment4_results.txt${NC}"
