#ifndef LFCPA_CALLSTRING_H
#define LFCPA_CALLSTRING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

class CallString {
    public:
        static CallString empty();
        CallString addCallSite(Instruction *) const;
        bool isNonCyclicPrefix(const CallString &) const;
        CallString createCyclicFromPrefix(const CallString &) const;
        bool matches(const CallString &) const;
        CallString(const CallString &other) : nonCyclic(other.nonCyclic), cyclic(other.cyclic) {}
        void dump() const;

        inline bool operator==(const CallString &C) const {
            return C.cyclic == cyclic && C.nonCyclic == nonCyclic;
        }

        inline bool isEmpty() const {
            return cyclic.empty() && nonCyclic.empty();
        }

        inline bool isCyclic() const {
            return !cyclic.empty();
        }

        inline int size() const {
            assert (!isCyclic());
            return nonCyclic.size();
        }

        inline Instruction *getLastCall() const {
            assert(!isCyclic());
            assert(!isEmpty());
            return nonCyclic.back();
        }

        inline bool containsCallTo(const Function *F) const {
            for (Instruction *I : nonCyclic)
                if (CallInst *CI = dyn_cast<CallInst>(I))
                    if (CI->getCalledFunction() == nullptr || CI->getCalledFunction() == F)
                        return true;
            for (Instruction *I : cyclic)
                if (CallInst *CI = dyn_cast<CallInst>(I))
                    if (CI->getCalledFunction() == nullptr || CI->getCalledFunction() == F)
                        return true;
            return false;
        }
    private:
        SmallVector<Instruction *, 8> nonCyclic;
        SmallVector<Instruction *, 8> cyclic;
        CallString () {}
};

#endif
