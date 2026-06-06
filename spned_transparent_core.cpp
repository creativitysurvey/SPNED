#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <sstream>
#include <sys/mman.h>
#include <cstring>
#include <iomanip>

// ==========================================
// 1. 核心代数域：区间结构 (Interval Domain)
// ==========================================
struct Interval {
    long long min_val, max_val;
    bool is_bottom;
    Interval() : min_val(0), max_val(0), is_bottom(true) {}
    Interval(long long min_v, long long max_v) : min_val(min_v), max_val(max_v), is_bottom(false) {}
    static Interval Top() { return Interval(-9999999, 9999999); }
};

// ==========================================
// 2. 增强的 AST 结构与 DSL 解析器
// ==========================================
enum StmtType { OP_ASSIGN_DIRECT, OP_ASSIGN_INDIRECT, OP_STORE };

struct Stmt {
    StmtType type;
    virtual bool verify(std::unordered_map<std::string, Interval>& env, long long buf_size) = 0;
    virtual ~Stmt() = default;
};

struct AssignDirect : public Stmt {
    std::string var;
    long long multiplier; // 动态乘数
    AssignDirect(std::string v, long long m) : var(v), multiplier(m) { type = OP_ASSIGN_DIRECT; }
    bool verify(std::unordered_map<std::string, Interval>& env, long long buf_size) override {
        env[var] = Interval(0, env["i"].max_val * multiplier);
        return true;
    }
};

struct AssignIndirect : public Stmt {
    std::string var;
    AssignIndirect(std::string v) : var(v) { type = OP_ASSIGN_INDIRECT; }
    bool verify(std::unordered_map<std::string, Interval>& env, long long buf_size) override {
        env[var] = Interval::Top(); // 状态爆炸
        return true;
    }
};

struct StoreSafe : public Stmt {
    std::string var;
    StoreSafe(std::string v) : var(v) { type = OP_STORE; }
    bool verify(std::unordered_map<std::string, Interval>& env, long long buf_size) override {
        if (env[var].is_bottom || env[var].min_val < 0 || env[var].max_val >= buf_size) return false;
        return true;
    }
};

// 真实的文本 DSL 解析（取代硬编码 AST）
std::vector<std::shared_ptr<Stmt>> parse_dsl(const std::string& source) {
    std::vector<std::shared_ptr<Stmt>> ast;
    std::istringstream stream(source);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("ASSIGN_DIRECT") != std::string::npos) {
            // 提取乘数 (模拟词法分析)
            size_t pos = line.find_last_of('*');
            long long mult = (pos != std::string::npos) ? std::stoll(line.substr(pos + 1)) : 1;
            ast.push_back(std::make_shared<AssignDirect>("idx", mult));
        } else if (line.find("ASSIGN_INDIRECT") != std::string::npos) {
            ast.push_back(std::make_shared<AssignIndirect>("idx"));
        } else if (line.find("STORE") != std::string::npos) {
            ast.push_back(std::make_shared<StoreSafe>("idx"));
        }
    }
    return ast;
}

// ==========================================
// 3. 动态 x86-64 JIT 编译器 (彻底干掉硬编码汇编)
// ==========================================
typedef int (*JITFunc)(int);

class DynamicJITEmitter {
private:
    std::vector<unsigned char> machine_code;

public:
    // 发射指令: mov eax, edi (将输入参数放入 eax)
    void emit_prologue() {
        machine_code.push_back(0x89); machine_code.push_back(0xF8);
    }
    
    // 发射指令: imul eax, eax, <imm32> (动态乘法)
    void emit_imul_eax(int multiplier) {
        machine_code.push_back(0x69); machine_code.push_back(0xC0);
        unsigned char* p = reinterpret_cast<unsigned char*>(&multiplier);
        for(int i=0; i<4; i++) machine_code.push_back(p[i]);
    }

    // 发射指令: ret (返回)
    void emit_epilogue() {
        machine_code.push_back(0xC3);
    }

    // 真正的编译核心：根据 AST 动态生成机器码
    JITFunc compile_ast(const std::vector<std::shared_ptr<Stmt>>& ast) {
        std::cout << "  [JIT Compiler] Translating AST to x86-64 Machine Code dynamically...\n";
        emit_prologue();

        for (const auto& stmt : ast) {
            if (stmt->type == OP_ASSIGN_DIRECT) {
                auto assign = static_cast<AssignDirect*>(stmt.get());
                std::cout << "  [JIT Compiler] Emitting IMUL instruction for multiplier: " << assign->multiplier << "\n";
                emit_imul_eax(assign->multiplier);
            }
            // STORE 阶段在实际硬件中会生成 mov 指令，此处用乘法结果模拟流水线
        }
        emit_epilogue();

        // 将动态生成的指令刷入可执行内存
        void* mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        std::memcpy(mem, machine_code.data(), machine_code.size());
        mprotect(mem, 4096, PROT_READ | PROT_EXEC);

        std::cout << "  [JIT Compiler] Hex dump of emitted instructions: ";
        for (auto b : machine_code) std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
        std::cout << std::dec << "\n";

        return (JITFunc)mem;
    }
};

// ==========================================
// 4. 核心评估流程
// ==========================================
int main() {
    std::cout << "=== SPNED True Dynamic JIT Engine (Artifact Refinement) ===\n\n";

    // 测试: 解析真实的 DSL，并通过动态 JIT 生成代码
    std::string safe_dsl = "ASSIGN_DIRECT idx i * 7\nSTORE idx\n";
    std::cout << "[1] Parsing Safe DSL Source:\n" << safe_dsl;
    auto ast = parse_dsl(safe_dsl);
    
    std::cout << "[2] Running Interval Verification...\n";
    std::unordered_map<std::string, Interval> env; env["i"] = Interval(0, 100);
    bool is_safe = true;
    for (auto& stmt : ast) if (!stmt->verify(env, 40000)) is_safe = false;
    std::cout << "  -> Verification: " << (is_safe ? "PASSED" : "FAILED") << "\n\n";

    if (is_safe) {
        std::cout << "[3] Triggering OS-level Dynamic JIT Compilation...\n";
        DynamicJITEmitter compiler;
        JITFunc native_decoder = compiler.compile_ast(ast);
        
        // 测试 JIT 函数：输入 10，AST 里的乘数是 7，期望输出 70
        int input = 10;
        int result = native_decoder(input);
        std::cout << "\n[4] Executing Naked JIT Code...\n";
        std::cout << "  -> Input: " << input << " | Dynamic Zero-copy Result: " << result << " (Expected: " << input * 7 << ")\n";
    }

    return 0;
}
