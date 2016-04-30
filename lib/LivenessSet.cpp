#include "llvm/Support/raw_ostream.h"

#include "LivenessSet.h"

void PointsToNodeSet::dump() const {
    bool first = true;
    for (auto *N : *this) {
        if (!first)
            errs() << ", ";
        first = false;
        errs() << N->getName();
    }
    errs() << "\n";
}

