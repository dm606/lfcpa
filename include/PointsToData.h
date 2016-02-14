#ifndef LFCPA_POINTSTODATA_H
#define LFCPA_POINTSTODATA_H

#include <set>

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Function.h"

#include "CallString.h"
#include "LivenessSet.h"
#include "PointsToNode.h"
#include "PointsToRelation.h"

using namespace llvm;

typedef DenseMap<const Instruction *, std::pair<LivenessSet *, PointsToRelation *>> IntraproceduralPointsTo;
typedef SmallVector<std::pair<CallString, IntraproceduralPointsTo *>, 8> ProcedurePointsTo;

bool arePointsToMapsEqual(Function *F, IntraproceduralPointsTo *a, IntraproceduralPointsTo *b);
IntraproceduralPointsTo *copyPointsToMap(IntraproceduralPointsTo *);

class PointsToData {
    public:
        PointsToData() {}
        ProcedurePointsTo *getAtFunction(const Function *) const;
        IntraproceduralPointsTo *getPointsTo(const CallString &, Function *);
        bool attemptMakeCyclicCallString(Function *, const CallString &, IntraproceduralPointsTo *);
        bool hasDataForFunction(const Function *) const;
        IntraproceduralPointsTo *getAtLongestPrefix(const Function *, const CallString &) const;
    private:
        DenseMap<const Function *, ProcedurePointsTo *> data;
};

#endif
