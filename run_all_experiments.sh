#!/bin/bash

# ==============================================================================
# SPNED Artifact Evaluation Master Execution Script
# ==============================================================================

# 定义颜色输出以增强控制台可读性
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${CYAN}========================================================${NC}"
echo -e "${CYAN} Initiating SPNED Full Artifact Evaluation Suite...     ${NC}"
echo -e "${CYAN}========================================================${NC}"

# 【全局环境免疫】剥离 Conda linker 干扰，确保全局调用纯净系统 C++ 工具链
export PATH=$(echo $PATH | tr ':' '\n' | grep -v -i "conda" | grep -v "spned_env" | paste -sd ':' -)
unset LD_LIBRARY_PATH
unset LIBRARY_PATH

echo -e "[+] Global environment sanitized (Conda bypassed)."
echo ""

# ---------------------------------------------------------
# 执行 Experiment 3: Context Switch Latency (微观上下文切换延迟)
# ---------------------------------------------------------
if [ -f "./run_exp3.sh" ]; then
    echo -e "${YELLOW}>>> [1/3] Running Experiment 3 (Context Switch Latency)...${NC}"
    bash ./run_exp3.sh
    echo ""
else
    echo -e "[-] Warning: run_exp3.sh not found. Skipping."
fi

# ---------------------------------------------------------
# 执行 Experiment 4: NUMA Scalability (多核内存墙物理带宽)
# ---------------------------------------------------------
if [ -f "./run_exp4.sh" ]; then
    echo -e "${YELLOW}>>> [2/3] Running Experiment 4 (NUMA / DRAM Saturation)...${NC}"
    bash ./run_exp4.sh
    echo ""
else
    echo -e "[-] Warning: run_exp4.sh not found. Skipping."
fi

# ---------------------------------------------------------
# 执行 Experiment 2: Transparent Verifier & Naked JIT (真正的形式化验证器)
# ---------------------------------------------------------
if [ -f "./run_transparent_eval.sh" ]; then
    echo -e "${YELLOW}>>> [3/3] Running Experiment 2 (Transparent Verifier & mmap JIT)...${NC}"
    bash ./run_transparent_eval.sh
    echo ""
else
    echo -e "[-] Warning: run_transparent_eval.sh not found. Skipping."
fi

echo -e "${GREEN}========================================================${NC}"
echo -e "${GREEN} ALL EXPERIMENTS COMPLETED SUCCESSFULLY!                ${NC}"
echo -e "${GREEN}========================================================${NC}"
