#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include "LivenessPointsTo.h"
#include "LivenessSet.h"

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

        // Pointer casts (including GEPs with indices that are all zero) do not
        // affect what is pointed to.
        const Value *A = LocA.Ptr->stripPointerCasts();
        const Value *B = LocB.Ptr->stripPointerCasts();

        // Some preliminary (and very fast!) checks.
        if (!A->getType()->isPointerTy() || !B->getType()->isPointerTy())
            return NoAlias;
        if (A == B)
            return MustAlias;

        LivenessSet ASet = analysis.getPointsToSet(A);
        LivenessSet BSet = analysis.getPointsToSet(B);

        // If either of the sets are empty, then we don't know what one of the
        // values can point to, and therefore we don't know if they can alias.
        if (ASet.empty() || BSet.empty())
            return AliasAnalysis::alias(LocA, LocB);

        // If the values may point to the same thing, then they may alias.
        for (PointsToNode *N : ASet)
            for (PointsToNode *M : BSet)
                if (M->isSubNodeOf(N) || N->isSubNodeOf(M))
                    return AliasAnalysis::alias(LocA, LocB);

        // If the values do not share any pointees then they cannot alias.
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
