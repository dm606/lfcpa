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
    SmallVector<std::tuple<CallString, const Function *, PointsToRelation>, 64> callData;
    void runOnModule(Module &);
    ProcedurePointsTo *getPointsTo(Function &) const;
    std::set<PointsToNode *> getPointsToSet(const Value *, bool &);
    static unsigned worklistIterations, timesRanOnFunction;
private:
    void insertNewPairs(PointsToRelation &, const Instruction *, PointsToRelation &);
    void subtractKill(const CallString &CS, LivenessSet &, const Instruction *, PointsToRelation &);
    bool isArgument(const Function *, const PointsToNode *);
    bool computeAin(const Instruction *, const Function *, PointsToRelation &, IntraproceduralPointsTo *);
    bool getCalledFunctions(SmallVector<const Function *, 8> &, const CallInst *, PointsToRelation &);
    LivenessSet findRelevantNodes(const CallInst *, LivenessSet &);
    void addAoutCalledDeclaration(PointsToRelation &, const CallInst *, PointsToRelation &);
    void addAoutAnalysableCalledFunction(PointsToRelation &, const Function *, const CallString &, const CallInst *, PointsToRelation &);
    bool computeAout(const CallString &, const Instruction *, PointsToRelation &, PointsToRelation &);
    std::set<PointsToNode *> getKillableDeclaration(const CallInst *, PointsToRelation &);
    PointsToRelation getCalledFunctionResult(const CallString &, const Function *);
    std::set<PointsToNode *> getReturnValues(const Function *);
    PointsToRelation replaceActualArgumentsWithFormal(const Function *, const CallInst *, PointsToRelation *);
    LivenessSet replaceFormalArgumentsWithActual(const CallString &CS, const Function *, const CallInst *, LivenessSet &, LivenessSet &);
    PointsToRelation replaceReturnValuesWithCallInst(const CallString &CS, const CallInst *, PointsToRelation &, std::set<PointsToNode *> &);
    void runOnFunction(const Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation &, SmallVector<std::tuple<const CallInst *, const Function *, PointsToRelation>, 8> &);
    bool runOnFunctionAt(const CallString &, const Function *, PointsToRelation &, bool);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *);
    LivenessSet getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
};

#endif
