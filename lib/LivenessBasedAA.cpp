#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/LivenessPointsTo.h"

using namespace llvm;


namespace {
struct LivenessBasedAA : public ModulePass, public AliasAnalysis {
    static char ID;

    LivenessPointsTo analysis;

    LivenessBasedAA() : ModulePass(ID) {}

    bool runOnModule(Module &M) override {
        InitializeAliasAnalysis(this, &M.getDataLayout());
        analysis.runOnModule(M);
        return false;
    }

    AliasResult alias(const MemoryLocation &LocA,
                      const MemoryLocation &LocB) override {
        if (LocA.Size == 0 || LocB.Size == 0)
            return NoAlias;

        const Value *A = LocA.Ptr->stripPointerCasts();
        const Value *B = LocB.Ptr->stripPointerCasts();

        if (!A->getType()->isPointerTy() || !B->getType()->isPointerTy())
            return NoAlias;

        if (A == B)
            return MustAlias;

        LivenessSet ASet = analysis.getPointsToSet(A);
        LivenessSet BSet = analysis.getPointsToSet(B);

        if (ASet.empty() || BSet.empty())
            return AliasAnalysis::alias(LocA, LocB);

        if (ASet.size() == 1 && BSet.size() == 1 && ASet == BSet)
            return MustAlias;

        for (PointsToNode *N : ASet)
            if (BSet.find(N) != BSet.end())
                return AliasAnalysis::alias(LocA, LocB);

        return NoAlias;
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AliasAnalysis::getAnalysisUsage(AU);
      AU.setPreservesAll();
    }

    void *getAdjustedAnalysisPointer(AnalysisID PI) override {
      if (PI == &AliasAnalysis::ID)
        return (AliasAnalysis*)this;
      return this;
    }
};
}



char LivenessBasedAA::ID = 0;
static RegisterPass<LivenessBasedAA> X("lfcpa", "Liveness-based alias analysis pass.", false, false);
static RegisterAnalysisGroup<AliasAnalysis> Y(X);
