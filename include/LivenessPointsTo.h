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
private:
    std::set<PointsToNode *> getRestrictedDef(Instruction *,
                                              PointsToRelation *,
                                              std::set<PointsToNode *> *);
    void insertPointedToBy(std::set<PointsToNode *> &,
                           Value *,
                           PointsToRelation *);
    std::set<PointsToNode *> getPointee(Instruction *, PointsToRelation *);
    void unionCrossProduct(PointsToRelation &,
                           std::set<PointsToNode *> &,
                           std::set<PointsToNode *> &);
    void subtractKill(std::set<PointsToNode *>&,
                      Instruction *,
                      PointsToRelation *);
    void unionRef(std::set<PointsToNode *>&,
                  Instruction *,
                  std::set<PointsToNode *>*,
                  PointsToRelation*);
    void unionRelationRestriction(PointsToRelation &,
                                  PointsToRelation *,
                                  std::set<PointsToNode *> *);
    void computeLout(Instruction *, LivenessSet * , IntraproceduralPointsTo *, PointsToRelation *, bool);
    bool computeAin(Instruction *, Function *, PointsToRelation *, LivenessSet *, IntraproceduralPointsTo *);
    std::pair<PointsToRelation *, LivenessSet*> getReachable(Function *, CallInst *, PointsToRelation *, LivenessSet *);
    void insertReachable(Function *, CallInst *, LivenessSet &, LivenessSet &, PointsToRelation *);
    void insertReachableDeclaration(CallInst *, LivenessSet &, PointsToRelation *);
    void insertReachablePT(CallInst *, PointsToRelation &, PointsToRelation &, PointsToRelation *, LivenessSet &);
    bool getCalledFunctionResult(const CallString &, Function *, std::pair<LivenessSet, PointsToRelation>&);
    LivenessSet getReturnValues(const Function *);
    void runOnFunction(Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation *, LivenessSet *, SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet *>, 8> &);
    bool runOnFunctionAt(const CallString &, Function *, PointsToRelation *, LivenessSet *);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *, std::set<PointsToNode *>*);
    std::set<PointsToNode *> getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
    LivenessSet globals;
};

#endif
