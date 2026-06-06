#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <chrono>
#include <algorithm>

// ==========================================
// 1. 核心代数域：区间结构 (Interval Domain)
// ==========================================
struct Interval {
    long long min_val;
    long long max_val;
    bool is_bottom; // 表示不可达状态

    Interval() : min_val(0), max_val(0), is_bottom(true) {}
    Interval(long long v) : min_val(v), max_val(v), is_bottom(false) {}
    Interval(long long min_v, long long max_v) : min_val(min_v), max_val(max_v), is_bottom(false) {}

    // 区间加法
    Interval operator+(const Interval& other) const {
        if (is_bottom || other.is_bottom) return Interval();
        return Interval(min_val + other.min_val, max_val + other.max_val);
    }
    // 区间乘法 (简化版，假设非负)
    Interval operator*(const Interval& other) const {
        if (is_bottom || other.is_bottom) return Interval();
        return Interval(min_val * other.min_val, max_val * other.max_val);
    }
    // 拓宽操作 (用于处理无法确定边界的动态加载)
    static Interval Top() { return Interval(-9999999, 9999999); }
};

// ==========================================
// 2. 抽象语法树 (AST) 与 IR 定义
// ==========================================
struct ASTNode {
    virtual ~ASTNode() = default;
};

struct Expr : public ASTNode {
    virtual Interval evaluate(std::unordered_map<std::string, Interval>& env) = 0;
};

struct ConstExpr : public Expr {
    long long val;
    ConstExpr(long long v) : val(v) {}
    Interval evaluate(std::unordered_map<std::string, Interval>& env) override { return Interval(val); }
};

struct VarExpr : public Expr {
    std::string name;
    VarExpr(std::string n) : name(n) {}
    Interval evaluate(std::unordered_map<std::string, Interval>& env) override { 
        return env.count(name) ? env[name] : Interval::Top(); 
    }
};

struct BinOpExpr : public Expr {
    std::string op;
    std::shared_ptr<Expr> left, right;
    BinOpExpr(std::string o, std::shared_ptr<Expr> l, std::shared_ptr<Expr> r) : op(o), left(l), right(r) {}
    Interval evaluate(std::unordered_map<std::string, Interval>& env) override {
        Interval l = left->evaluate(env);
        Interval r = right->evaluate(env);
        if (op == "+") return l + r;
        if (op == "*") return l * r;
        return Interval::Top();
    }
};

struct LoadIndirectExpr : public Expr {
    // 模拟从数据字典中加载不可预测的值
    Interval evaluate(std::unordered_map<std::string, Interval>& env) override { return Interval::Top(); }
};

// ==========================================
// 3. 语句与验证逻辑 (Statements)
// ==========================================
struct Stmt : public ASTNode {
    virtual bool execute_and_verify(std::unordered_map<std::string, Interval>& env, long long buffer_size) = 0;
};

struct AssignStmt : public Stmt {
    std::string var_name;
    std::shared_ptr<Expr> expr;
    AssignStmt(std::string n, std::shared_ptr<Expr> e) : var_name(n), expr(e) {}
    bool execute_and_verify(std::unordered_map<std::string, Interval>& env, long long buffer_size) override {
        env[var_name] = expr->evaluate(env);
        return true;
    }
};

struct StoreStmt : public Stmt {
    std::shared_ptr<Expr> index_expr;
    StoreStmt(std::shared_ptr<Expr> idx) : index_expr(idx) {}
    bool execute_and_verify(std::unordered_map<std::string, Interval>& env, long long buffer_size) override {
        Interval idx = index_expr->evaluate(env);
        // 【绝对核心】：真正的数学验证！不是掷骰子！
        if (idx.min_val < 0 || idx.max_val >= buffer_size) {
            return false; // 内存越界，验证失败！
        }
        return true;
    }
};

struct ForStmt : public Stmt {
    std::string iter_var;
    long long start, end;
    std::vector<std::shared_ptr<Stmt>> body;
    
    ForStmt(std::string iv, long long s, long long e) : iter_var(iv), start(s), end(e) {}
    void add_stmt(std::shared_ptr<Stmt> s) { body.push_back(s); }

    bool execute_and_verify(std::unordered_map<std::string, Interval>& env, long long buffer_size) override {
        // 循环不变量：将迭代变量绑定为安全的区间 [start, end-1]
        env[iter_var] = Interval(start, end - 1);
        for (auto& stmt : body) {
            if (!stmt->execute_and_verify(env, buffer_size)) return false;
        }
        return true;
    }
};

// ==========================================
// 4. 构建真实的验证负载
// ==========================================

// Case 1: 完美安全的 FSST 序列化解压 (无随机性，必然通过)
std::shared_ptr<Stmt> build_fsst_safe_ast(long long tuple_count) {
    auto loop = std::make_shared<ForStmt>("i", 0, tuple_count);
    auto offset = std::make_shared<BinOpExpr>("*", std::make_shared<VarExpr>("i"), std::make_shared<ConstExpr>(4));
    loop->add_stmt(std::make_shared<AssignStmt>("idx", offset));
    loop->add_stmt(std::make_shared<StoreStmt>(std::make_shared<VarExpr>("idx")));
    return loop;
}

// Case 2: 危险的 BtrBlocks 字典解码 (间接寻址导致状态爆炸，必然失败)
std::shared_ptr<Stmt> build_btrblocks_unsafe_ast(long long tuple_count) {
    auto loop = std::make_shared<ForStmt>("i", 0, tuple_count);
    loop->add_stmt(std::make_shared<AssignStmt>("dict_offset", std::make_shared<LoadIndirectExpr>()));
    loop->add_stmt(std::make_shared<StoreStmt>(std::make_shared<VarExpr>("dict_offset")));
    return loop;
}

// ==========================================
// 5. 执行实验
// ==========================================
int main() {
    std::cout << "========================================================\n";
    std::cout << "[+] SPNED Abstract Interpreter Initialized (Interval Domain)\n";
    std::cout << "========================================================\n";

    long long tuple_count = 10000;
    long long buffer_size = tuple_count * 4; 

    // 测试 1：SPNED 安全代码验证
    std::cout << "[Test 1] Parsing & Verifying FSST Safe Decoder AST...\n";
    auto fsst_ast = build_fsst_safe_ast(tuple_count);
    std::unordered_map<std::string, Interval> env1;
    
    auto start1 = std::chrono::high_resolution_clock::now();
    bool fsst_safe = fsst_ast->execute_and_verify(env1, buffer_size);
    auto end1 = std::chrono::high_resolution_clock::now();
    
    std::cout << "    -> Verification Result: " << (fsst_safe ? "PASSED (Provably Safe)" : "FAILED (Unsafe)") << "\n";
    std::cout << "    -> Verification Latency: " << std::chrono::duration<double, std::micro>(end1 - start1).count() << " us\n\n";

    // 测试 2：eBPF 在遇到间接寻址时的必然失败
    std::cout << "[Test 2] Parsing & Verifying BtrBlocks Unsafe Dictionary AST...\n";
    auto btr_ast = build_btrblocks_unsafe_ast(tuple_count);
    std::unordered_map<std::string, Interval> env2;

    auto start2 = std::chrono::high_resolution_clock::now();
    bool btr_safe = btr_ast->execute_and_verify(env2, buffer_size);
    auto end2 = std::chrono::high_resolution_clock::now();

    std::cout << "    -> Verification Result: " << (btr_safe ? "PASSED (Provably Safe)" : "FAILED (Bounds Unprovable -> Rejected!)") << "\n";
    std::cout << "    -> FRR Emergence: Static analyzer correctly rejected due to Interval::Top() explosion.\n";
    std::cout << "    -> Verification Latency: " << std::chrono::duration<double, std::micro>(end2 - start2).count() << " us\n";

    return 0;
}
