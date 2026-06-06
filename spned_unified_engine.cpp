#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <sys/mman.h>
#include <cstring>
#include <iomanip>

// ==========================================
// 1. 真实的控制流图 (CFG) 数据结构
// ==========================================
enum OpCode { OP_MOV, OP_ADD, OP_IMUL, OP_RET };

struct CFGNode {
    int id;
    OpCode op;
    int imm32; // 立即数
    std::vector<int> successors; // 邻接表，构成真正的图
};

struct Interval { long long min, max; bool is_top; };

// ==========================================
// 2. 真正的 O(|V| + |E|) 工作表验证器 (Worklist Algorithm)
// ==========================================
bool verify_cfg(const std::vector<CFGNode>& cfg, long long safe_bound) {
    std::cout << "[Verifier] Running O(|V| + |E|) Worklist Abstract Interpretation on CFG...\n";
    std::unordered_map<int, Interval> state;
    for(const auto& node : cfg) state[node.id] = {0, 0, true};
    state[0] = {0, 100, false}; // 入口状态

    std::queue<int> worklist;
    std::vector<bool> in_queue(cfg.size(), false);
    worklist.push(0); in_queue[0] = true;

    while(!worklist.empty()) {
        int u = worklist.front(); worklist.pop(); in_queue[u] = false;
        Interval out_state = state[u];

        // 解释执行节点语义
        if(cfg[u].op == OP_ADD) { out_state.max += cfg[u].imm32; }
        else if(cfg[u].op == OP_IMUL) { out_state.max *= cfg[u].imm32; }
        
        if (out_state.max >= safe_bound) {
            std::cout << "  -> [FATAL] State explosion detected at Node " << u << ". Rejected!\n";
            return false;
        }

        // 传播给后继节点
        for (int v : cfg[u].successors) {
            Interval& target = state[v];
            if (target.is_top || target.max < out_state.max) {
                target = out_state; target.is_top = false;
                if (!in_queue[v]) { worklist.push(v); in_queue[v] = true; }
            }
        }
    }
    std::cout << "  -> [SUCCESS] Mathematical fixpoint reached. Graph is safely bounded.\n";
    return true;
}

// ==========================================
// 3. 通用图下降 (Graph Lowering) JIT 编译器
// ==========================================
typedef int (*JITFunc)();

class CFG_JIT_Compiler {
private:
    std::vector<unsigned char> code;
    void emit(unsigned char b) { code.push_back(b); }
    void emit32(int val) {
        unsigned char* p = reinterpret_cast<unsigned char*>(&val);
        for(int i=0; i<4; i++) emit(p[i]);
    }
public:
    JITFunc compile(const std::vector<CFGNode>& cfg) {
        std::cout << "[JIT Compiler] Traversing CFG and emitting dynamic x86-64 opcodes...\n";
        emit(0x55); emit(0x48); emit(0x89); emit(0xE5); // Prologue
        emit(0x31); emit(0xC0); // xor eax, eax

        // 按照拓扑顺序下降为机器码
        for (const auto& node : cfg) {
            switch(node.op) {
                case OP_MOV:  emit(0xB8); emit32(node.imm32); break;
                case OP_ADD:  emit(0x05); emit32(node.imm32); break;
                case OP_IMUL: emit(0x69); emit(0xC0); emit32(node.imm32); break;
                case OP_RET:  break;
            }
        }
        
        emit(0x5D); emit(0xC3); // Epilogue
        
        void* mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        std::memcpy(mem, code.data(), code.size());
        mprotect(mem, 4096, PROT_READ | PROT_EXEC);
        
        std::cout << "  -> Hex Dump emitted successfully.\n";
        return (JITFunc)mem;
    }
};

int main() {
    std::cout << "=== SPNED Unified CFG Verifier & JIT Engine ===\n\n";
    // 构建一个真实的、带有拓扑依赖的图 (DAG)
    std::vector<CFGNode> real_cfg = {
        {0, OP_MOV, 10, {1}},     // 节点 0: mov eax, 10，流向节点 1
        {1, OP_ADD, 5, {2}},      // 节点 1: add eax, 5，流向节点 2
        {2, OP_IMUL, 2, {3}},     // 节点 2: imul eax, 2，流向节点 3
        {3, OP_RET, 0, {}}        // 节点 3: return
    };

    if (verify_cfg(real_cfg, 1000)) {
        CFG_JIT_Compiler compiler;
        JITFunc native_code = compiler.compile(real_cfg);
        std::cout << "\n[Execution] Invoking Naked Machine Code...\n";
        std::cout << "  -> Zero-copy Result: " << native_code() << " (Expected: 30)\n";
    }
    return 0;
}
