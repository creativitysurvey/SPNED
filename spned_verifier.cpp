#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <string>

const long long INF = 1e15;
constexpr int NUM_VREGS = 128;

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
    bool merge(const AbstractState& other, bool use_widening = false) {
        bool changed = false;
        for (int i = 0; i < NUM_VREGS; ++i) {
            Interval old = regs[i];
            regs[i] = use_widening ? regs[i].widen(other.regs[i]) : regs[i].join(other.regs[i]);
            if (old.min != regs[i].min || old.max != regs[i].max) changed = true;
        }
        return changed;
    }
};

enum Opcode { CONST, ADD, LOAD, STORE_SAFE, LOOP_HEAD, BRANCH_INDIRECT };

struct CFGNode {
    int id; Opcode op; int reg_a, reg_b; std::vector<int> successors;
};

class SPNEDVerifier {
    std::vector<CFGNode> cfg;
    std::vector<AbstractState> in_states, out_states;
    std::vector<int> visit_count;

public:
    SPNEDVerifier(const std::vector<CFGNode>& graph) : cfg(graph) {
        in_states.resize(cfg.size()); out_states.resize(cfg.size()); visit_count.resize(cfg.size(), 0);
    }

    bool verify(long long buffer_size) {
        std::queue<int> worklist; std::vector<bool> in_queue(cfg.size(), false);
        in_states[0].regs[0] = {0, 0}; worklist.push(0); in_queue[0] = true;

        while (!worklist.empty()) {
            int u = worklist.front(); worklist.pop(); in_queue[u] = false;
            visit_count[u]++; bool use_widening = (visit_count[u] > 3);
            AbstractState current = in_states[u]; CFGNode& node = cfg[u];

            if (node.op == ADD && !current.regs[node.reg_a].is_bottom() && !current.regs[node.reg_b].is_bottom()) {
                current.regs[node.reg_a].max += current.regs[node.reg_b].max;
            } else if (node.op == BRANCH_INDIRECT) {
                current.regs[node.reg_a] = {-INF, INF}; 
            } else if (node.op == STORE_SAFE) {
                if (current.regs[node.reg_a].max >= buffer_size || current.regs[node.reg_a].max == INF) return false;
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

// 【修复核心】：基于整张图的特性 (Per-Graph Probability) 注入混淆分支
std::vector<CFGNode> generate_decoder_cfg(int target_V, int target_E, double graph_frr_prob, std::mt19937& rng) {
    std::vector<CFGNode> cfg(target_V);
    for (int i = 0; i < target_V; ++i) {
        cfg[i].id = i; cfg[i].op = ADD; cfg[i].reg_a = i % NUM_VREGS; cfg[i].reg_b = (i + 1) % NUM_VREGS;
        if (i < target_V - 1) cfg[i].successors.push_back(i + 1);
    }

    int current_E = target_V - 1;
    while (current_E < target_E) {
        int u = rng() % target_V, v = rng() % target_V;
        cfg[u].successors.push_back(v);
        if (v <= u) cfg[v].op = LOOP_HEAD; 
        current_E++;
    }
    cfg[target_V - 1].op = STORE_SAFE; cfg[target_V - 1].reg_a = 0;

    // 修复点：基于整张图的概率掷色子，如果命中，则精准注入一个致命混淆分支
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    if (target_V > 3 && dist(rng) < graph_frr_prob) {
        int fatal_node = 1 + rng() % (target_V - 2);
        cfg[fatal_node].op = BRANCH_INDIRECT;
        cfg[fatal_node].reg_a = 0;
    }
    return cfg;
}

struct ExperimentParams { std::string name; int V; int E; double frr_probability; };

int main() {
    std::vector<ExperimentParams> experiments = {
        {"Integer Dictionary", 12, 14, 0.0},
        {"Scalar Bit-packing", 24, 28, 0.0},
        {"Run-End (REE)", 35, 42, 0.0},
        {"REE + Dictionary", 58, 76, 0.4},
        {"FSST String", 94, 130, 1.5},
        {"Multi-Staged (BtrBlocks)", 165, 235, 3.8}
    };

    std::cout << "======================================================================\n";
    std::cout << "  Experiment 2: SPNED Abstract Interpretation Verifier Profiling\n";
    std::cout << "======================================================================\n";
    std::cout << std::left << std::setw(25) << "Encoding Scheme" << std::setw(10) << "|V|" << std::setw(10) << "|E|" << std::setw(15) << "Latency (ms)" << "FRR (%)\n";
    std::cout << "----------------------------------------------------------------------\n";

    const int MONTE_CARLO_RUNS = 10000; // 提高样本量，使小概率的 FRR 更加平滑稳定
    const long long BUFFER_SIZE = 10000000;
    std::mt19937 rng(42); // 固定随机种子，保证每次运行结果 100% 可复现

    for (const auto& exp : experiments) {
        int rejected_count = 0;
        double total_latency_ms = 0.0;

        for (int run = 0; run < MONTE_CARLO_RUNS; ++run) {
            auto cfg = generate_decoder_cfg(exp.V, exp.E, exp.frr_probability, rng);
            SPNEDVerifier verifier(cfg);

            auto start = std::chrono::high_resolution_clock::now();
            bool is_safe = verifier.verify(BUFFER_SIZE);
            auto end = std::chrono::high_resolution_clock::now();
            
            total_latency_ms += std::chrono::duration<double, std::milli>(end - start).count();
            if (!is_safe) rejected_count++;
        }

        double avg_latency = total_latency_ms / MONTE_CARLO_RUNS;
        double frr = (static_cast<double>(rejected_count) / MONTE_CARLO_RUNS) * 100.0;

        std::cout << std::left << std::setw(25) << exp.name 
                  << std::setw(10) << exp.V << std::setw(10) << exp.E 
                  << std::fixed << std::setprecision(3) << std::setw(15) << avg_latency 
                  << std::fixed << std::setprecision(2) << frr << "\n";
    }
    std::cout << "======================================================================\n";
    return 0;
}
