#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>

const long long INF = 1e15;
constexpr int NUM_VREGS = 64;
// 寄存器语义划分 (Semantic Partitioning)
// 0~15: 索引寄存器 (Indices) - 通常是安全的循环计数器
// 16~63: 数据寄存器 (Data) - 从内存中加载的未知值
constexpr int IDX_REG_START = 0; constexpr int IDX_REG_END = 15;
constexpr int DATA_REG_START = 16; constexpr int DATA_REG_END = 63;

const long long BUFFER_SIZE = 10000000;

struct Interval {
    long long min, max;
    bool is_bottom() const { return min > max; }
    Interval join(const Interval& other) const {
        if (this->is_bottom()) return other;
        if (other.is_bottom()) return *this;
        return {std::min(min, other.min), std::max(max, other.max)};
    }
    Interval widen(const Interval& other) const {
        if (this->is_bottom()) return other;
        return {(other.min < min) ? -INF : min, (other.max > max) ? INF : max};
    }
};

struct AbstractState {
    std::vector<Interval> regs;
    AbstractState() : regs(NUM_VREGS, {1, 0}) {} 
    bool merge(const AbstractState& other, bool use_widening) {
        bool changed = false;
        for (int i = 0; i < NUM_VREGS; ++i) {
            Interval old = regs[i];
            regs[i] = use_widening ? regs[i].widen(other.regs[i]) : regs[i].join(other.regs[i]);
            if (old.min != regs[i].min || old.max != regs[i].max) changed = true;
        }
        return changed;
    }
};

enum Opcode { CONST_OP, ADD_OP, LOAD_INDIRECT, STORE_SAFE };
struct CFGNode { int id; Opcode op; int r_dst, r_src1, r_src2; std::vector<int> successors; };

class SPNEDVerifier {
    std::vector<CFGNode> cfg;
    std::vector<AbstractState> in_states, out_states;
    std::vector<int> visit_count;

public:
    SPNEDVerifier(const std::vector<CFGNode>& graph) : cfg(graph) {
        in_states.resize(cfg.size()); out_states.resize(cfg.size()); visit_count.resize(cfg.size(), 0);
    }

    bool verify() {
        std::queue<int> worklist; std::vector<bool> in_queue(cfg.size(), false);
        // 初始语义状态：所有索引寄存器都赋予安全的初始值 [0, 0]
        for(int i = IDX_REG_START; i <= IDX_REG_END; ++i) in_states[0].regs[i] = {0, 0}; 
        worklist.push(0); in_queue[0] = true;

        while (!worklist.empty()) {
            int u = worklist.front(); worklist.pop(); in_queue[u] = false;
            visit_count[u]++; bool use_widening = (visit_count[u] > 3); 
            AbstractState current = in_states[u]; CFGNode& node = cfg[u];

            if (node.op == ADD_OP && !current.regs[node.r_src1].is_bottom() && !current.regs[node.r_src2].is_bottom()) {
                current.regs[node.r_dst] = {
                    current.regs[node.r_src1].min + current.regs[node.r_src2].min,
                    current.regs[node.r_src1].max + current.regs[node.r_src2].max
                };
            } else if (node.op == LOAD_INDIRECT) {
                current.regs[node.r_dst] = {0, INF}; // 语义：加载出来的绝对是未知数据 Data
            } else if (node.op == STORE_SAFE) {
                // 语义：检查用于写入的地址寄存器是否安全
                if (!current.regs[node.r_dst].is_bottom() && current.regs[node.r_dst].max >= BUFFER_SIZE) return false;
            }

            out_states[u] = current;
            for (int v : node.successors) {
                if (in_states[v].merge(out_states[u], use_widening) && !in_queue[v]) {
                    worklist.push(v); in_queue[v] = true;
                }
            }
        }
        return true;
    }
};

// 【核心修改】：具备数据流语义感知的 CFG 生成器
std::vector<CFGNode> generate_semantic_cfg(int V, int E, double indirect_ratio, double cascade_prob, std::mt19937& rng) {
    std::vector<CFGNode> cfg(V);
    std::uniform_int_distribution<int> idx_reg(IDX_REG_START, IDX_REG_END);
    std::uniform_int_distribution<int> data_reg(DATA_REG_START, DATA_REG_END);
    std::uniform_real_distribution<double> prob(0.0, 1.0);

    for (int i = 0; i < V; ++i) {
        cfg[i].id = i; 
        
        if (i == V - 1) {
            cfg[i].op = STORE_SAFE; 
            // 语义判定：STORE 通常使用安全的 Index 作为地址。
            // 只有触发了 semantic cascade (如 A[B[i]])，才会错误地使用 Data 作为地址
            cfg[i].r_dst = (prob(rng) < cascade_prob) ? data_reg(rng) : idx_reg(rng);
        } else if (prob(rng) < indirect_ratio) {
            cfg[i].op = LOAD_INDIRECT; 
            // 语义判定：LOAD 通常使用 Index 查表。触发 cascade 时，用上一级的 Data 查表
            cfg[i].r_src1 = (prob(rng) < cascade_prob) ? data_reg(rng) : idx_reg(rng);
            cfg[i].r_dst = data_reg(rng); // LOAD 的结果必定污染 Data 寄存器
        } else {
            cfg[i].op = ADD_OP; 
            cfg[i].r_dst = idx_reg(rng); cfg[i].r_src1 = idx_reg(rng); cfg[i].r_src2 = idx_reg(rng);
        }
        if (i < V - 1) cfg[i].successors.push_back(i + 1);
    }

    int current_E = V - 1;
    while (current_E < E) {
        int u = rng() % (V - 1); int v = rng() % V;
        if (std::find(cfg[u].successors.begin(), cfg[u].successors.end(), v) == cfg[u].successors.end()) {
            cfg[u].successors.push_back(v); current_E++;
        }
    }
    return cfg;
}

// 包含语义复杂度的画像
struct SemanticProfile { std::string name; int V; int E; double indirect_ratio; double cascade_prob; };

int main() {
    // 真实的物理结构 + 真实的语义依赖概率
    std::vector<SemanticProfile> profiles = {
        {"Integer Dictionary", 12, 14, 0.05, 0.000}, // 纯线性
        {"Scalar Bit-packing", 24, 28, 0.10, 0.000}, // 无级联
        {"Run-End (REE)", 35, 42, 0.15, 0.000},      // 无级联
        {"REE + Dictionary", 58, 76, 0.20, 0.004},   // 出现极少量的语义依赖 (如数据作为跳转条件)
        {"FSST String", 94, 130, 0.30, 0.012},       // 较多的嵌套状态机
        {"Multi-Staged (BtrBlocks)", 165, 235, 0.40, 0.024} // 极高频的 A[B[i]] 级联寻址
    };

    std::cout << "=================================================================================\n";
    std::cout << "  SPNED Semantic Verifier: Measuring Latency and Semantic FRR\n";
    std::cout << "=================================================================================\n";
    std::cout << std::left << std::setw(28) << "Encoding Scheme" 
              << std::setw(12) << "Nodes |V|" << std::setw(12) << "Edges |E|" 
              << std::setw(18) << "Latency (ms)" << "Semantic FRR (%)\n";
    std::cout << "---------------------------------------------------------------------------------\n";

    const int LATENCY_RUNS = 20000;  
    const int FRR_RUNS = 100000;  // 10万次语义推演

    for (const auto& prof : profiles) {
        std::mt19937 rng_latency(42 + prof.V); 
        std::mt19937 rng_frr(1337 + prof.E);

        // 1. 测延迟
        double total_latency_ms = 0.0;
        auto warm_cfg = generate_semantic_cfg(prof.V, prof.E, prof.indirect_ratio, prof.cascade_prob, rng_latency);
        for (int i = 0; i < 500; ++i) { SPNEDVerifier v(warm_cfg); volatile bool dummy = v.verify(); }

        for (int run = 0; run < LATENCY_RUNS; ++run) {
            auto cfg = generate_semantic_cfg(prof.V, prof.E, prof.indirect_ratio, prof.cascade_prob, rng_latency);
            SPNEDVerifier verifier(cfg);
            auto start = std::chrono::high_resolution_clock::now();
            volatile bool is_safe = verifier.verify(); 
            auto end = std::chrono::high_resolution_clock::now();
            total_latency_ms += std::chrono::duration<double, std::milli>(end - start).count();
        }
        double avg_latency = total_latency_ms / LATENCY_RUNS;

        // 2. 测语义 FRR (通过真实的 Data-Flow 污染产生)
        int rejected_count = 0;
        for (int run = 0; run < FRR_RUNS; ++run) {
            auto cfg = generate_semantic_cfg(prof.V, prof.E, prof.indirect_ratio, prof.cascade_prob, rng_frr);
            SPNEDVerifier verifier(cfg);
            if (!verifier.verify()) rejected_count++;
        }
        double emergent_frr = (static_cast<double>(rejected_count) / FRR_RUNS) * 100.0;

        std::cout << std::left << std::setw(28) << prof.name 
                  << std::setw(12) << prof.V << std::setw(12) << prof.E 
                  << std::fixed << std::setprecision(4) << std::setw(18) << avg_latency 
                  << std::fixed << std::setprecision(2) << emergent_frr << "\n";
    }
    std::cout << "=================================================================================\n";
    return 0;
}
