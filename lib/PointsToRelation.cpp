#include "llvm/Support/raw_ostream.h"

#include "PointsToRelation.h"

void PointsToRelation::dump() const {
    bool first = true;
    for (auto P : s) {
        if (!first)
            errs() << ", ";
        first = false;
        errs() << P.first->getName() << "-->" << P.second->getName();
    }
    errs() << "\n";
}
