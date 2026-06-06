#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <sys/mman.h>
#include <cstring>
#include <iomanip>

enum OpCode { OP_MOV, OP_ADD, OP_IMUL, OP_INDIRECT, OP_RET };

struct CFGNode { int id; OpCode op; int imm32; std::vector<int> successors; };
struct Interval { long long min, max; bool is_top; };

bool verify_cfg(const std::vector<CFGNode>& cfg, long long safe_bound) {
    std::unordered_map<int, Interval> state;
    for(const auto& node : cfg) state[node.id] = {0, 0, true};
    state[0] = {0, 100, false};

    std::queue<int> worklist; std::vector<bool> in_queue(cfg.size(), false);
    worklist.push(0); in_queue[0] = true;

    while(!worklist.empty()) {
        int u = worklist.front(); worklist.pop(); in_queue[u] = false;
        Interval out_state = state[u];

        if(cfg[u].op == OP_ADD) { out_state.max += cfg[u].imm32; }
        else if(cfg[u].op == OP_IMUL) { out_state.max *= cfg[u].imm32; }
        else if(cfg[u].op == OP_INDIRECT) { out_state.is_top = true; out_state.max = 9999999; }
        
        if (out_state.max >= safe_bound) {
            std::cout << "  -> [FATAL] State explosion detected at Node " << u << " (Interval Max: TOP). Rejected!\n";
            return false;
        }
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

typedef int (*JITFunc)();
class CFG_JIT_Compiler {
private:
    std::vector<unsigned char> code;
    void emit(unsigned char b) { code.push_back(b); }
    void emit32(int val) { unsigned char* p = reinterpret_cast<unsigned char*>(&val); for(int i=0; i<4; i++) emit(p[i]); }
public:
    JITFunc compile(const std::vector<CFGNode>& cfg) {
        emit(0x55); emit(0x48); emit(0x89); emit(0xE5); emit(0x31); emit(0xC0);
        for (const auto& node : cfg) {
            switch(node.op) {
                case OP_MOV:  emit(0xB8); emit32(node.imm32); break;
                case OP_ADD:  emit(0x05); emit32(node.imm32); break;
                case OP_IMUL: emit(0x69); emit(0xC0); emit32(node.imm32); break;
                default: break;
            }
        }
        emit(0x5D); emit(0xC3);
        void* mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        std::memcpy(mem, code.data(), code.size());
        mprotect(mem, 4096, PROT_READ | PROT_EXEC);
        return (JITFunc)mem;
    }
};

int main() {
    std::cout << "=== SPNED Unified CFG Verifier & JIT Engine ===\n\n";
    
    std::cout << "[Phase 1] Parsing Safe CFG (Deterministic Bounds)...\n";
    std::vector<CFGNode> safe_cfg = { {0, OP_MOV, 10, {1}}, {1, OP_ADD, 5, {2}}, {2, OP_IMUL, 2, {3}}, {3, OP_RET, 0, {}} };
    if (verify_cfg(safe_cfg, 1000)) {
        std::cout << "  -> JIT Lowering Triggered...\n";
        CFG_JIT_Compiler compiler; JITFunc native = compiler.compile(safe_cfg);
        std::cout << "  -> Zero-copy Result: " << native() << " (Expected: 30)\n\n";
    }

    std::cout << "[Phase 2] Parsing Unsafe CFG (Indirect Branching / State Explosion)...\n";
    std::vector<CFGNode> bad_cfg = { {0, OP_MOV, 10, {1}}, {1, OP_INDIRECT, 0, {2}}, {2, OP_RET, 0, {}} };
    verify_cfg(bad_cfg, 1000);

    return 0;
}
