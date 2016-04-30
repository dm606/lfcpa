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
    SmallVector<std::tuple<CallString, const Function *, PointsToRelation, LivenessSet, bool>, 64> callData;
    void runOnModule(Module &);
    ProcedurePointsTo *getPointsTo(Function &) const;
    std::set<PointsToNode *> getPointsToSet(const Value *, bool &);
    static unsigned worklistIterations, timesRanOnFunction;
private:
    void insertNewPairs(PointsToRelation &, const Instruction *, PointsToRelation &, LivenessSet &);
    void subtractKill(const CallString &CS, LivenessSet &, const Instruction *, PointsToRelation &);
    void unionRef(LivenessSet &, const Instruction *, LivenessSet &, PointsToRelation &);
    void computeLout(const Instruction *, LivenessSet &, IntraproceduralPointsTo &);
    bool isArgument(const Function *, const PointsToNode *);
    bool computeAin(const Instruction *, const Function *, PointsToRelation &, LivenessSet &, IntraproceduralPointsTo *, bool InsertAtFirstInstruction);
    bool getCalledFunctions(SmallVector<const Function *, 8> &, const CallInst *, PointsToRelation &);
    void addLinCalledDeclaration(LivenessSet &, const CallString &, const CallInst *, LivenessSet &);
    void addLinAnalysableCalledFunction(LivenessSet &, const Function *, const CallString &, const CallInst *, LivenessSet &, LivenessSet &);
    LivenessSet findRelevantNodes(const CallInst *, LivenessSet &);
    bool computeLin(const CallString &, const Instruction *, PointsToRelation &, LivenessSet &, LivenessSet &);
    void addAoutCalledDeclaration(PointsToRelation &, const CallInst *, PointsToRelation &, LivenessSet &);
    void addAoutAnalysableCalledFunction(PointsToRelation &, const Function *, const CallString &, const CallInst *, PointsToRelation &, LivenessSet &);
    bool computeAout(const CallString &, const Instruction *, PointsToRelation &, PointsToRelation &, LivenessSet &);
    PointsToNodeSet getKillableDeclaration(const CallInst *, PointsToRelation &);
    std::pair<LivenessSet, PointsToRelation> getCalledFunctionResult(const CallString &, const Function *);
    PointsToNodeSet getReturnValues(const Function *);
    LivenessSet computeFunctionExitLiveness(const CallInst *, LivenessSet *);
    PointsToRelation replaceActualArgumentsWithFormal(const Function *, const CallInst *, PointsToRelation *);
    LivenessSet replaceFormalArgumentsWithActual(const CallString &CS, const Function *, const CallInst *, LivenessSet &, LivenessSet &);
    PointsToRelation replaceReturnValuesWithCallInst(const CallInst *, PointsToRelation &, PointsToNodeSet &, LivenessSet &);
    void runOnFunction(const Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation &, LivenessSet &, bool, SmallVector<std::tuple<const CallInst *, const Function *, PointsToRelation, LivenessSet, bool>, 8> &);
    bool runOnFunctionAt(const CallString &, const Function *, PointsToRelation &, LivenessSet &, bool, bool);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *, LivenessSet *);
    LivenessSet getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
};

#endif
