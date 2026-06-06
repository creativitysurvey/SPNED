#!/bin/bash
# Exit immediately if a command exits with a non-zero status
set -e

# Define colors for pretty output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${GREEN}======================================================================${NC}"
echo -e "${GREEN}          SPNED Artifact Evaluation: Experiment 2 (Table 2)           ${NC}"
echo -e "${GREEN}======================================================================${NC}"
echo ""

# Bypass Conda phantom linker issues by forcing system PATH
export SYS_PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

echo -e "${CYAN}[1/4] Compiling Frontend CFG Profiler (measure_cfg.cpp)...${NC}"
env PATH=$SYS_PATH g++ -O3 -std=c++17 measure_cfg.cpp -o measure_cfg
echo "      -> Compilation successful."

echo -e "${CYAN}[2/4] Compiling Semantic Verifier Suite (measure_semantic_table2.cpp)...${NC}"
env PATH=$SYS_PATH g++ -O3 -std=c++17 measure_semantic_table2.cpp -o measure_semantic_table2
echo "      -> Compilation successful."
echo ""

echo -e "${GREEN}[3/4] Initializing Monte Carlo CFG Synthesizer with Baseline Profiles...${NC}"
./measure_cfg
echo ""

echo -e "${GREEN}[4/4] Executing Semantic Verification Suite (Profiling Latency & FRR)...${NC}"
echo -e "${CYAN}      -> Note: Running 100,000 Monte-Carlo semantic sweeps. Please wait ~10 seconds...${NC}"

# Run the suite and tee the output to both console and a text file
./measure_semantic_table2 | tee experiment2_results.txt

echo ""
echo -e "${GREEN}======================================================================${NC}"
echo -e "${GREEN}  SUCCESS: All benchmarks completed!                                  ${NC}"
echo -e "${GREEN}  A copy of the final Table 2 data has been saved to:                 ${NC}"
echo -e "${GREEN}  ./experiment2_results.txt                                           ${NC}"
echo -e "${GREEN}======================================================================${NC}"
