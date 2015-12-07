#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "include/LivenessPointsTo.h"

using namespace llvm;

namespace {
struct TestPass : public FunctionPass {
    static char ID;

    LivenessPointsTo analysis;

    TestPass() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
        errs() << "TestPass: ";
        errs().write_escaped(F.getName()) << "\n";
        analysis.runOnFunction(F);

        errs() << "Finished running analysis on function.\n";

        for (BasicBlock &BB : F) {
            for (Instruction &I : BB) {
                auto sv = analysis.getPointsTo(I);
                errs() << "Points-to information at "
                       << BB.getName() << "," << I.getName() << ": ";
                bool first = true;
                for (auto i : *sv) {
                    if (first) {
                        errs() << i.first->getName() << " --> "
                               << i.second->getName();
                        first = false;
                    }
                    else {
                        errs() << ", " << i.first->getName() << " --> "
                               << i.second->getName();
                    }
                }
                errs() << "\n";
            }
        }

        return false;
    }
};

char TestPass::ID;
static RegisterPass<TestPass> X("test-pass", "Test pass for LFCPA.");
}
