#include <iostream>
#include <vector>
#include <string>
#include <sys/mman.h>
#include <cstring>
#include <sstream>

enum class OpType { ADD, MUL };
struct ASTNode { OpType type; int32_t value; };
struct Interval { long long min, max; }; // 抽象解释的区间域

std::vector<ASTNode> parse_expression(const std::string& expr) {
    std::vector<ASTNode> ast;
    std::stringstream ss(expr);
    std::string token;
    while (ss >> token) {
        if (token == "+") { int val; ss >> val; ast.push_back({OpType::ADD, val}); }
        else if (token == "*") { int val; ss >> val; ast.push_back({OpType::MUL, val}); }
    }
    return ast;
}

// 【学术核心】：基于区间数学的抽象解释器 (Abstract Interpretation)
// 在 JIT 编译之前，静态证明指令流是否会越界！
bool static_safety_proof(const std::vector<ASTNode>& ast, Interval input_domain, Interval& final_state) {
    final_state = input_domain;
    for (const auto& node : ast) {
        if (node.type == OpType::ADD) {
            final_state.min += node.value; final_state.max += node.value;
        } else if (node.type == OpType::MUL) {
            // 简化处理正数乘法
            final_state.min *= node.value; final_state.max *= node.value;
        }
    }
    // 假设系统安全边界为 100000
    const long long SAFE_LIMIT = 100000;
    return final_state.max <= SAFE_LIMIT;
}

class DynamicX86Emitter {
private:
    uint8_t* code; size_t offset;
    void emit8(uint8_t b) { code[offset++] = b; }
    void emit32(uint32_t v) { std::memcpy(code + offset, &v, 4); offset += 4; }
public:
    DynamicX86Emitter() : offset(0) { code = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); }
    ~DynamicX86Emitter() { munmap(code, 4096); }

    void* compile_naked(const std::vector<ASTNode>& ast) {
        offset = 0;
        emit8(0x55); emit8(0x48); emit8(0x89); emit8(0xE5); // push rbp; mov rbp, rsp
        emit8(0x89); emit8(0xF8);                           // mov eax, edi
        
        for (const auto& node : ast) {
            if (node.type == OpType::ADD) { emit8(0x05); emit32(node.value); }
            else if (node.type == OpType::MUL) { emit8(0x69); emit8(0xC0); emit32(node.value); }
        }
        
        emit8(0x5D); emit8(0xC3); // pop rbp; ret
        mprotect(code, 4096, PROT_READ | PROT_EXEC);
        return (void*)code;
    }
};

int main(int argc, char** argv) {
    std::string input_expr = (argc > 1) ? argv[1] : "+ 10 * 2";
    std::cout << "[+] Parsing AST for workload: x " << input_expr << "\n";
    std::vector<ASTNode> ast = parse_expression(input_expr);
    
    std::cout << "[+] Triggering Interval-based Abstract Interpretation...\n";
    Interval initial_state = {0, 100}; // 假设输入 x 的物理区间是 [0, 100]
    Interval final_state;
    
    if (static_safety_proof(ast, initial_state, final_state)) {
        std::cout << "  -> [STATIC PROOF PASSED] Final Bounds: [" << final_state.min << ", " << final_state.max << "]\n";
        std::cout << "  -> Result is within safe limit. Emitting NAKED x86-64 machine code (No Bounds Checks)...\n";
        
        DynamicX86Emitter jit;
        typedef int (*JitFunc)(int);
        JitFunc compiled_func = (JitFunc)jit.compile_naked(ast);
        
        int result = compiled_func(5);
        std::cout << "  -> Execution Result: " << result << "\n\n";
    } else {
        std::cout << "  -> [STATIC PROOF FAILED] Final bounds exceed safety limits: [" << final_state.min << ", " << final_state.max << "]\n";
        std::cout << "  -> Fallback to VM Sandbox. Compilation Aborted!\n\n";
    }
    return 0;
}
