#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "LivenessPointsTo.h"

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

    bool areAllSubNodes(const std::set<PointsToNode *> A, const std::set<PointsToNode *> B) {
        for (auto N : A)
            for (auto M : B)
                if (!N->isSubNodeOf(M))
                    return false;
        return true;
    }

    AliasResult getResult(const MemoryLocation &LocA,
                          const MemoryLocation &LocB) {
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
        if (isa<UndefValue>(A) || isa<UndefValue>(B)) {
            // We don't know what undef points to, but we are allowed to assume
            // that it doesn't alias with anything.
            return NoAlias;
        }

        bool allowMustAlias = true;
        std::set<PointsToNode *> ASet = analysis.getPointsToSet(A, allowMustAlias);
        std::set<PointsToNode *> BSet = analysis.getPointsToSet(B, allowMustAlias);

        // If either of the sets are empty, then we don't know what one of the
        // values can point to, and therefore we don't know if they can alias.
        if (ASet.empty() || BSet.empty())
            return MayAlias;

        std::pair<const PointsToNode *, SmallVector<uint64_t, 4>> address;
        bool possibleMustAlias = allowMustAlias, foundAddress = false;
        for (PointsToNode *N : ASet) {
            if (possibleMustAlias) {
                auto currentAddress = N->getAddress();
                if (foundAddress && address != currentAddress)
                    possibleMustAlias = false;
                else if (!foundAddress) {
                    foundAddress = true;
                    address = currentAddress;
                }
            }
        }
        for (PointsToNode *N : BSet) {
            if (possibleMustAlias) {
                auto currentAddress = N->getAddress();
                // ASet contains at least one element.
                assert(foundAddress);
                if (address != currentAddress)
                    possibleMustAlias = false;
            }
        }

        if (possibleMustAlias) {
            // This happens when ASet and BSet each contain exactly one node,
            // and that node is the same (mod trailing zeros).
            return MustAlias;
        }

        if (allowMustAlias) {
            // If all of the nodes in one set are subnodes of all of the nodes in
            // the other, then they partially alias.
            if (areAllSubNodes(ASet, BSet))
                return PartialAlias;
            if (areAllSubNodes(BSet, ASet))
                return PartialAlias;
        }

        for (PointsToNode *N : ASet)
            for (PointsToNode *M : BSet)
                if (M->isSubNodeOf(N) || N->isSubNodeOf(M))
                    return MayAlias;

        // If the values do not share any pointees then they cannot alias.
        return NoAlias;
    }

   AliasResult alias(const MemoryLocation &LocA,
                      const MemoryLocation &LocB) override {
        AliasResult result = getResult(LocA, LocB);

        return result == MayAlias ? AliasAnalysis::alias(LocA, LocB) : result;
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
static RegisterPass<LivenessBasedAA> X("lfcpa", "Liveness-based alias analysis", false, false);
static RegisterAnalysisGroup<AliasAnalysis> Y(X);

static void registerClangPass(const PassManagerBuilder &, legacy::PassManagerBase &PM) {
    PM.add(new LivenessBasedAA());
}
static RegisterStandardPasses RegisterClangPass(PassManagerBuilder::EP_ModuleOptimizerEarly, registerClangPass);
