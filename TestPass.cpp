#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
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

        for (BasicBlock &BB : F) {
            SmallVectorImpl<std::pair<PointsToNode, PointsToNode>>& sv = analysis.getPointsTo(BB);
            errs() << "Points-to information at BB " << BB.getName() << ": ";
            bool first = true;
            for (auto i : sv) {
                if (first) {
                    errs() << 0;
                    first = false;
                }
                else {
                    errs() << ", " << 0;
                }
            }
        }

        return false;
    }
};

char TestPass::ID;
static RegisterPass<TestPass> X("test-pass", "Test pass for LFCPA.");
}
