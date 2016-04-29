#include "llvm/Support/raw_ostream.h"

#include "PointsToRelation.h"

void PointsToRelation::dump() const {
    bool first = true;
    for (auto *N : PointsToNode::AllNodes) {
        if (N->singlePointee())
            continue;

        for (auto I = pointee_begin(N), E = pointee_end(N); I != E; ++I) {
            if (!first)
                errs() << ", ";
            first = false;
            errs() << N->getName() << "-->" << (*I)->getName();
        }
    }
    errs() << "\n";
}
