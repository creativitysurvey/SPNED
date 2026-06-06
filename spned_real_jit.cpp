#include <iostream>
#include <vector>
#include <sys/mman.h>
#include <cstring>
#include <cstdint>

enum class OpType { ADD, MUL };
struct ASTNode { OpType type; int32_t value; };

class RealX86Emitter {
private:
    uint8_t* code;
    size_t size;
    size_t offset;

    void emit8(uint8_t b) { code[offset++] = b; }
    void emit32(uint32_t v) { std::memcpy(code + offset, &v, 4); offset += 4; }

public:
    RealX86Emitter() : offset(0), size(4096) {
        code = (uint8_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    }
    ~RealX86Emitter() { munmap(code, size); }

    void* compile_ast(const std::vector<ASTNode>& ast) {
        offset = 0;
        // Prologue
        emit8(0x55); // push rbp
        emit8(0x48); emit8(0x89); emit8(0xE5); // mov rbp, rsp
        
        // Loop initialization: xor rcx, rcx
        emit8(0x48); emit8(0x31); emit8(0xC9); 

        size_t loop_start = offset;

        // cmp rcx, rsi
        emit8(0x48); emit8(0x39); emit8(0xF1); 
        emit8(0x0F); emit8(0x8D); // jge loop_end
        size_t jge_offset = offset;
        emit32(0); 

        // mov eax, dword ptr [rdi + rcx*4]
        emit8(0x8B); emit8(0x04); emit8(0x8F);

        // 动态遍历 AST 发射算术机器码
        for (const auto& node : ast) {
            if (node.type == OpType::ADD) {
                emit8(0x05); emit32(node.value); // add eax, imm32
            } else if (node.type == OpType::MUL) {
                emit8(0x69); emit8(0xC0); emit32(node.value); // imul eax, eax, imm32
            }
        }

        // mov dword ptr [rdx + rcx*4], eax
        emit8(0x89); emit8(0x04); emit8(0x8A);
        // inc rcx
        emit8(0x48); emit8(0xFF); emit8(0xC1);

        // jmp loop_start
        emit8(0xE9);
        int32_t jmp_rel = (int32_t)loop_start - (int32_t)(offset + 4);
        emit32(jmp_rel);

        // Backpatch
        size_t loop_end = offset;
        uint32_t exit_rel = loop_end - (jge_offset + 4);
        std::memcpy(code + jge_offset, &exit_rel, 4);

        // Epilogue
        emit8(0x5D); // pop rbp
        emit8(0xC3); // ret

        // 赋予操作系统级别的执行权限
        mprotect(code, size, PROT_READ | PROT_EXEC); 
        return (void*)code;
    }
};

int main() {
    std::cout << "[+] SPNED Real JIT Compilation from Dynamic AST...\n";
    std::vector<ASTNode> my_ast = { {OpType::ADD, 10}, {OpType::MUL, 2} };

    RealX86Emitter jit;
    typedef void (*JitFunc)(int*, int, int*);
    std::cout << "  -> Emitting Raw x86-64 Machine Code into Memory...\n";
    JitFunc func = (JitFunc)jit.compile_ast(my_ast);

    int in_data[] = {1, 2, 3};
    int out_data[3] = {0};
    
    std::cout << "  -> Executing Naked Machine Code...\n";
    func(in_data, 3, out_data); 

    std::cout << "  -> Result [0]: " << out_data[0] << " (Expected: (1+10)*2 = 22)\n";
    std::cout << "  -> Result [1]: " << out_data[1] << " (Expected: (2+10)*2 = 24)\n";
    std::cout << "  -> Result [2]: " << out_data[2] << " (Expected: (3+10)*2 = 26)\n\n";
    return 0;
}
