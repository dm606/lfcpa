#ifndef LFCPA_CALLSTRING_H
#define LFCPA_CALLSTRING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

class CallString {
    public:
        static CallString empty();
        CallString addCallSite(const Instruction *) const;
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

        inline const Instruction *getLastCall() const {
            assert(!isCyclic());
            assert(!isEmpty());
            return nonCyclic.back();
        }

        inline bool containsCallIn(const Function *F) const {
            for (const Instruction *I : nonCyclic)
                if (const CallInst *CI = dyn_cast<CallInst>(I))
                    if (CI->getParent()->getParent() == F)
                        return true;
            for (const Instruction *I : cyclic)
                if (const CallInst *CI = dyn_cast<CallInst>(I))
                    if (CI->getParent()->getParent() == F)
                        return true;
            return false;
        }

        inline bool reachedMoreThanOnce(const Function *F) const {
            assert(!isCyclic());
            bool foundCall = false;
            const CallInst *Last = nullptr;
            for (const Instruction *I : nonCyclic) {
                if (const CallInst *CI = dyn_cast<CallInst>(I)) {
                    if (CI->getParent()->getParent() == F) {
                        if (foundCall)
                            return true;
                        else
                            foundCall = true;
                    }
                    Last = CI;
                }
            }

            if (foundCall && Last != nullptr) {
                // This is imprecise because Last may not actually be a call to
                // F if getCalledFunction is nullptr, but it is safe.
                return Last->getCalledFunction() == nullptr || Last->getCalledFunction() == F;
            }

            return false;
        }
    private:
        SmallVector<const Instruction *, 8> nonCyclic;
        SmallVector<const Instruction *, 8> cyclic;
        CallString () {}
};

#endif
