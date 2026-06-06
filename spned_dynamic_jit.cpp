#include <iostream>
#include <vector>
#include <string>
#include <sys/mman.h>
#include <cstring>
#include <sstream>

enum class OpType { ADD, MUL };
struct ASTNode { OpType type; int32_t value; };

// 动态解析器：将命令行传进来的字符串转化为 AST
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

class DynamicX86Emitter {
private:
    uint8_t* code; size_t offset;
    void emit8(uint8_t b) { code[offset++] = b; }
    void emit32(uint32_t v) { std::memcpy(code + offset, &v, 4); offset += 4; }
public:
    DynamicX86Emitter() : offset(0) { code = (uint8_t*)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); }
    ~DynamicX86Emitter() { munmap(code, 4096); }

    void* compile(const std::vector<ASTNode>& ast) {
        offset = 0;
        emit8(0x55); emit8(0x48); emit8(0x89); emit8(0xE5); // push rbp; mov rbp, rsp
        emit8(0x89); emit8(0xF8);                           // mov eax, edi
        
        // 动态遍历并生成指令
        for (const auto& node : ast) {
            if (node.type == OpType::ADD) { emit8(0x05); emit32(node.value); }
            else if (node.type == OpType::MUL) { emit8(0x69); emit8(0xC0); emit32(node.value); }
        }
        
        emit8(0x5D); emit8(0xC3);                           // pop rbp; ret
        mprotect(code, 4096, PROT_READ | PROT_EXEC);
        return (void*)code;
    }
};

int main(int argc, char** argv) {
    std::string input_expr = (argc > 1) ? argv[1] : "+ 10 * 2 + 5";
    std::cout << "[+] Compiling dynamic expression at runtime: x " << input_expr << "\n";
    std::vector<ASTNode> ast = parse_expression(input_expr);
    DynamicX86Emitter jit;
    
    typedef int (*JitFunc)(int);
    JitFunc compiled_func = (JitFunc)jit.compile(ast);
    
    int test_val = 5;
    int result = compiled_func(test_val);
    std::cout << "[+] JIT Execution Result for input (" << test_val << "): " << result << "\n";
    return 0;
}
