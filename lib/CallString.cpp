#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/CallString.h"

CallString CallString::empty() {
    return CallString();
}

CallString CallString::addCallSite(Instruction *I) const {
    assert(isa<CallInst>(I) && "Only call instructions can be call sites.");
    assert(!isRecursive() && "Call sites can only be added to non-recursive call instructions.");
    CallString result = *this;
    result.nonRecursive.push_back(I);
    return result;
}

bool CallString::isNonRecursivePrefix(const CallString &S) const {
    assert (!isRecursive() && "Both call strings must be non-recursive.");
    assert (!S.isRecursive() && "Both call strings must be non-recursive.");
    auto thisIter = nonRecursive.begin();
    auto thisEnd = nonRecursive.end();
    for (auto &I : S.nonRecursive) {
        if (thisIter == thisEnd) {
            // S is longer!
            return false;
        }
        else if (I != *thisIter) {
            // S contains a different element.
            return false;
        }
        else
            thisIter++;
    }
    return thisIter != thisEnd;
}

CallString CallString::createRecursiveFromPrefix(const CallString &S) const {
    assert (!isRecursive() && "Both call strings must be non-recursive.");
    assert (!S.isRecursive() && "Both call strings must be non-recursive.");
    auto thisIter = nonRecursive.begin();
    auto thisEnd = nonRecursive.end();
    for (auto &I : S.nonRecursive) {
        assert (thisIter != thisEnd && "The prefix is too long.");
        assert (I == *thisIter && "The call string is not a prefix.");
        thisIter++;
    }

    assert(thisIter != thisEnd && "The prefix is too long.");
    CallString result = S;
    result.recursive = SmallVector<Instruction *, 8>(thisIter, thisEnd);
    return result;
}

bool CallString::matches(const CallString &S) const {
    assert(!S.isRecursive() && "Can only test if non-recursive call-strings match.");

    auto iter = S.nonRecursive.begin();
    auto end = S.nonRecursive.end();
    for (auto &I : nonRecursive) {
        if (iter == end) {
            // S isn't long enough!
            return false;
        }
        else if (I != *iter) {
            // S contains a different element.
            return false;
        }
        else
            iter++;
    }

    if (isRecursive()) {
        while (iter != end) {
            for (auto &I : recursive) {
                if (iter == end) {
                    // S has an incomplete recursive part at the end.
                    return false;
                }
                else if (I != *iter) {
                    // S contains a different element.
                    return false;
                }
                else
                    iter++;
            }
        }

        return true;
    }
    else
        return iter == end;
}

void CallString::dump() const {
    bool first = true;
    for (auto &I : nonRecursive) {
        if (!first)
            errs() << ", ";
        I->dump();
        first = false;
    }

    if (isRecursive()) {
        if (!first)
            errs() << ", ";
        errs() << "[";
        first = true;
        for (auto &I : recursive) {
            if (!first)
                errs() << ", ";
            I->dump();
            first = false;
        }
        errs() << "]*";
    }

    errs() << "\n";
}

