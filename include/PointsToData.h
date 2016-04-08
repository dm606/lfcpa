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

typedef std::tuple<CallInst *, Function *, PointsToRelation *, bool> CallData;
typedef DenseMap<const Instruction *, PointsToRelation *> IntraproceduralPointsTo;
typedef SmallVector<std::tuple<CallString, IntraproceduralPointsTo *, PointsToRelation>, 8> ProcedurePointsTo;

bool arePointsToMapsEqual(const Function *F, IntraproceduralPointsTo *a, IntraproceduralPointsTo &b);
IntraproceduralPointsTo copyPointsToMap(IntraproceduralPointsTo *);

class PointsToData {
    public:
        PointsToData() {}
        ProcedurePointsTo *getAtFunction(const Function *) const;
        IntraproceduralPointsTo *getPointsTo(const CallString &, const Function *, PointsToRelation &, bool &);
        bool attemptMakeCyclicCallString(const Function *, const CallString &, IntraproceduralPointsTo *);
        bool hasDataForFunction(const Function *) const;
        IntraproceduralPointsTo *get(const Function *, const CallString &) const;
    private:
        DenseMap<const Function *, ProcedurePointsTo *> data;
};

#endif
