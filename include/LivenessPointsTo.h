#ifndef LFCPA_LIVENESSPOINTSTO_H
#define LFCPA_LIVENESSPOINTSTO_H

#include <set>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToData.h"
#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

using namespace llvm;

class LivenessPointsTo {
public:
    void runOnModule(Module &);
    ProcedurePointsTo *getPointsTo(Function &) const;
    std::set<PointsToNode *> getPointsToSet(const Value *, bool &);
private:
    LivenessSet getRestrictedDef(Instruction *, PointsToRelation *, LivenessSet *);
    void insertPointedToBy(std::set<PointsToNode *> &, Value *, PointsToRelation *);
    std::set<PointsToNode *> getPointee(Instruction *, PointsToRelation *);
    void subtractKill(LivenessSet &, Instruction *, PointsToRelation *);
    void unionRef(LivenessSet &, Instruction *, LivenessSet *, PointsToRelation *);
    void unionRelationRestriction(PointsToRelation &, PointsToRelation *, LivenessSet *);
    void computeLout(Instruction *, LivenessSet * , IntraproceduralPointsTo *, PointsToRelation *, bool);
    bool computeAin(Instruction *, Function *, PointsToRelation *, LivenessSet *, IntraproceduralPointsTo *);
    LivenessSet* getReachable(Function *, CallInst *, PointsToRelation *, LivenessSet *);
    PointsToRelation * getReachablePT(Function *, CallInst *, PointsToRelation *);
    void insertReachable(Function *, CallInst *, LivenessSet &, LivenessSet &, PointsToRelation *);
    void insertReachableDeclaration(CallInst *, LivenessSet &, PointsToRelation *);
    void insertReachablePT(CallInst *, PointsToRelation &, PointsToRelation &, PointsToRelation *, LivenessSet &);
    bool getCalledFunctionResult(const CallString &, Function *, std::pair<LivenessSet, PointsToRelation>&);
    LivenessSet getReturnValues(const Function *);
    void runOnFunction(Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation *, LivenessSet *, SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet *>, 8> &);
    bool runOnFunctionAt(const CallString &, Function *, PointsToRelation *, LivenessSet *);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *, LivenessSet *);
    LivenessSet getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
    LivenessSet globals;
};

#endif
