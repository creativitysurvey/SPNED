# SPNED: Static-Proof Native Execution of Decoders - Evaluation Artifact

## 1. 目的与作用
本代码仓库包含了 SPNED 架构的实验评估（Artifact Evaluation）源码与微基准测试（Micro-benchmarks）套件。其主要作用是测量和对比数据解码与过滤任务在不同执行架构下的物理吞吐量、验证延迟及上下文切换开销。

测试用例对比了以下三种执行环境：
1. **SPNED**：基于抽象语法树（AST）与区间分析（Interval Analysis）的动态 x86-64 机器码生成器。
2. **WebAssembly (Wasm)**：基于 `wasmtime-c-api` 的线性内存沙盒执行环境。
3. **eBPF**：基于 IOVisor `uBPF` 的用户态虚拟机与 JIT 执行环境。

## 2. 系统与环境要求
*   **操作系统**：Linux (必须为 x86_64 架构，以支持本实验的底层机器码发射机制)。
*   **编译器**：支持 C++14 标准的 GCC/G++；用于交叉编译 eBPF 字节码的 Clang/LLVM 工具链。
*   **依赖库**：
    *   `wasmtime-c-api` (Wasm 运行时)
    *   `ubpf` (eBPF 用户态虚拟机)

## 3. 下载与安装
请通过 Git 克隆本仓库及其依赖组件到本地环境：

```bash
git clone [https://github.com/creativitysurvey/SPNED.git](https://github.com/creativitysurvey/SPNED.git)
cd SPNED
```
*(注：请确保 `wasmtime-c-api` 和 `ubpf` 已正确放置在项目根目录下，或提前编译好相应的静态/动态链接库。)*

## 4. 编译与配置
项目中包含自动化构建脚本。在编译前，建议确保系统环境变量中没有与其冲突的第三方链接器（如 Conda 环境自带的 `ld`）。

**方法一：使用自动化脚本编译**
```bash
# 赋予脚本执行权限
chmod +x run_all_experiments.sh

# 运行自动化脚本（包含编译与环境检查步骤）
./run_all_experiments.sh
```

**方法二：手动编译核心基准测试 (Exp 1)**
如需手动编译主测试文件，请指定正确的头文件与库文件路径：
```bash
# 获取 uBPF 的配置与库文件路径
UBPF_CONF_DIR=$(dirname $(find $(pwd)/ubpf -name "ubpf_config.h" | head -n 1))
UBPF_LIB=$(find $(pwd)/ubpf/build -name "libubpf.a" | head -n 1)

# 使用 g++ 编译
g++ -O3 -std=c++14 measure_exp1_ultimate.cpp \
    -I./wasmtime-c-api/include -L./wasmtime-c-api/lib -lwasmtime -Wl,-rpath,$(pwd)/wasmtime-c-api/lib \
    -I./ubpf/vm/inc -I$UBPF_CONF_DIR $UBPF_LIB -o measure_exp1_bin -lm -lpthread -ldl -lrt
```

## 5. 运行方式
编译完成后，可在终端直接执行生成的二进制文件以查看测试结果。

**基准测试 1：绝对吞吐量对比 (Throughput)**
测试 SPNED、Wasm 与 eBPF 在顺序访问 (Sequential) 与条件过滤 (Selective) 模式下的数据处理吞吐率（单位：MT/s）。
```bash
./measure_exp1_bin
```

**基准测试 2：AST 区间验证器功能测试 (Verifier)**
测试基于 AST 的区间推导逻辑及动态机器码发射的正确性。
```bash
g++ -O3 -std=c++14 true_ast_jit.cpp -o true_ast_jit_bin
./true_ast_jit_bin
```

## 6. 核心文件结构说明
*   `measure_exp1_ultimate.cpp`：主基准测试源码，包含了多架构吞吐量对比逻辑。
*   `true_ast_jit.cpp`：微型 AST 驱动 JIT 引擎与静态区间验证器的独立实现。
*   `ebpf_payload.c`：用于编译生成 `BPF_ALU64` 字节码的 C 源码文件。
*   `run_all_experiments.sh`：项目全局编译与执行的入口脚本。
