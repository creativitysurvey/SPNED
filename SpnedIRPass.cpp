#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
  // ==============================================================================
  // SPNED Frontend Profiler Pass (Instruction-Level Granularity)
  // ==============================================================================
  struct SpnedFrontendPass : public FunctionPass {
    static char ID;
    SpnedFrontendPass() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
        int v_nodes = 0;
        int e_edges = 0;
        int indirect_loads = 0;
        int total_loads = 0;

        // 遍历当前函数的所有基本块 (Basic Blocks)
        for (BasicBlock &BB : F) {
            int bb_size = BB.size();
            if (bb_size == 0) continue;

            // 1. 测量节点数 |V|：SPNED 使用指令级粒度，每条指令为一个 Node
            v_nodes += bb_size; 
            
            // 2. 测量边数 |E|：严格符合图论定义的拓扑连线
            // a. 基本块内部：指令与指令之间的顺序执行边 (Fall-through edges)
            e_edges += (bb_size - 1);

            // b. 基本块之间：由 Terminator 产生的控制流跳转边
            const Instruction *Term = BB.getTerminator();
            if (Term) {
                e_edges += Term->getNumSuccessors();
            }

            // 3. 语义分析：扫描数据流以识别“间接级联寻址 (A[B[i]])”
            for (Instruction &I : BB) {
                if (auto *Load = dyn_cast<LoadInst>(&I)) {
                    total_loads++;
                    Value *Ptr = Load->getPointerOperand();
                    
                    if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
                        for (Use &U : GEP->indices()) {
                            if (isa<LoadInst>(U.get())) {
                                indirect_loads++;
                                break;
                            }
                        }
                    }
                }
            }
        }

        double cascade_prob = total_loads > 0 ? (double)indirect_loads / total_loads : 0.0;

        errs() << "==================================================\n";
        errs() << " SPNED Profile Export - Function: " << F.getName() << "\n";
        errs() << "==================================================\n";
        errs() << "  Measured Nodes (|V|)       : " << v_nodes << "\n";
        errs() << "  Measured Edges (|E|)       : " << e_edges << "\n";
        errs() << "  Semantic Cascade Prob (%)  : " << (cascade_prob * 100.0) << "%\n";
        errs() << "--------------------------------------------------\n\n";

        return false; // 仅做静态分析提取，不修改原始 IR
    }
  };
}

char SpnedFrontendPass::ID = 0;
static RegisterPass<SpnedFrontendPass> X("spned-profile", "Extract SPNED CFG Profiles and Semantic Data-flow", false, false);

static void registerSpnedPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    PM.add(new SpnedFrontendPass());
}
static RegisterStandardPasses RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible, registerSpnedPass);
