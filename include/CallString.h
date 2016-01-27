#ifndef LFCPA_CALLSTRING_H
#define LFCPA_CALLSTRING_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"

using namespace llvm;

class CallString {
    public:
        static CallString empty();
        CallString addCallSite(Instruction *) const;
        bool isNonRecursivePrefix(const CallString &) const;
        CallString createRecursiveFromPrefix(const CallString &) const;
        bool matches(const CallString &) const;
        CallString(const CallString &other) : nonRecursive(other.nonRecursive), recursive(other.recursive) {}
        void dump() const;

        inline bool operator==(const CallString &C) const {
            return C.recursive == recursive && C.nonRecursive == nonRecursive;
        }

        inline bool isEmpty() const {
            return recursive.empty() && nonRecursive.empty();
        }

        inline bool isRecursive() const {
            return !recursive.empty();
        }

        inline int size() const {
            assert (!isRecursive());
            return nonRecursive.size();
        }
    private:
        SmallVector<Instruction *, 8> nonRecursive;
        SmallVector<Instruction *, 8> recursive;
        CallString () {}
};

#endif
