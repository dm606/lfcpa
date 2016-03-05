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
    enum YesNoMaybe { Yes, No, Maybe };
    void runOnModule(Module &);
    ProcedurePointsTo *getPointsTo(Function &) const;
    std::set<PointsToNode *> getPointsToSet(const Value *, bool &);
    static unsigned worklistIterations, timesRanOnFunction;
private:
    void insertNewPairs(PointsToRelation &, Instruction *, PointsToRelation *, LivenessSet *);
    void subtractKill(const CallString &CS, LivenessSet &, Instruction *, PointsToRelation *);
    void unionRef(LivenessSet &, Instruction *, LivenessSet *, PointsToRelation *);
    void computeLout(Instruction *, LivenessSet * , IntraproceduralPointsTo *);
    bool computeAin(Instruction *, Function *, PointsToRelation *, LivenessSet *, IntraproceduralPointsTo *, bool InsertAtFirstInstruction);
    void insertReachableDeclaration(const CallString &, CallInst *, LivenessSet &, LivenessSet &, PointsToRelation *, YesNoMaybe &);
    bool getCalledFunctionResult(const CallString &, Function *, std::pair<LivenessSet, PointsToRelation>&);
    std::set<PointsToNode *> getReturnValues(const Function *);
    LivenessSet computeFunctionExitLiveness(CallInst *, LivenessSet *);
    PointsToRelation *replaceActualArgumentsWithFormal(Function *, CallInst *, PointsToRelation *);
    LivenessSet replaceFormalArgumentsWithActual(const CallString &CS, Function *, CallInst *, LivenessSet &, LivenessSet *);
    PointsToRelation replaceReturnValuesWithCallInst(CallInst *, PointsToRelation &, std::set<PointsToNode *> &, LivenessSet *);
    void runOnFunction(Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation *, LivenessSet &, bool, SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet, bool>, 8> &);
    bool runOnFunctionAt(const CallString &, Function *, PointsToRelation *, LivenessSet &, bool, bool);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *, LivenessSet *);
    LivenessSet getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
};

#endif
