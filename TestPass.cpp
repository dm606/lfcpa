#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "include/CallString.h"
#include "include/LivenessPointsTo.h"
#include "include/PointsToData.h"

using namespace llvm;

namespace {
struct TestPass : public ModulePass {
    static char ID;

    LivenessPointsTo analysis;

    TestPass() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        analysis.runOnModule(M);
        errs() << "\n";

        for (Function &F : M) {
            if (F.isDeclaration())
                continue;
            ProcedurePointsTo data = *analysis.getPointsTo(F);
            for (auto &P : data) {
                errs() << "\n";
                errs() << "Function: " << F.getName() << "\n";
                errs() << "Call string: ";
                P.first.dump();
                IntraproceduralPointsTo *pt = P.second;

                for (BasicBlock &BB : F) {
                    errs() << BB.getName() << ":\n";
                    for (Instruction &I : BB) {
                        auto sv = pt->find(&I)->second;
                        auto l = sv.first;
                        auto p = sv.second;
                        errs() << "Lin: \033[1;31m";
                        l->dump();
                        errs() << "\033[0m";
                        I.dump();
                        errs() << "Aout: \033[1;32m";
                        p->dump();
                        errs() << "\033[0m";
                    }
                }
            }
        }
        errs().flush();
        return false;
    }
};

char TestPass::ID;
static RegisterPass<TestPass> X("test-pass", "Test pass for LFCPA.");
}
