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
    typedef SmallVector<PointsToNode *, 16> GlobalVector;
    LivenessSet getRestrictedDef(Instruction *, PointsToRelation *, LivenessSet *);
    void insertPointedToBy(std::set<PointsToNode *> &, Value *, PointsToRelation *);
    std::set<PointsToNode *> getPointee(Instruction *, PointsToRelation *);
    void subtractKill(const CallString &CS, LivenessSet &, Instruction *, PointsToRelation *);
    void unionRef(LivenessSet &, Instruction *, LivenessSet *, PointsToRelation *);
    void computeLout(Instruction *, LivenessSet * , IntraproceduralPointsTo *, PointsToRelation *, bool, const GlobalVector &);
    bool computeAin(Instruction *, Function *, PointsToRelation *, LivenessSet *, IntraproceduralPointsTo *, bool InsertAtFirstInstruction);
    LivenessSet *getReachable(Function *, CallInst *, PointsToRelation *, LivenessSet *, GlobalVector &);
    PointsToRelation *getReachablePT(const CallString &, Function *, CallInst *, PointsToRelation *, GlobalVector &, YesNoMaybe &);
    void insertReachable(Function *, CallInst *, LivenessSet &, LivenessSet &, PointsToRelation *, GlobalVector &);
    void insertReachableDeclaration(const CallString &, CallInst *, LivenessSet &, LivenessSet &, PointsToRelation *, YesNoMaybe &);
    void insertReachablePT(CallInst *, PointsToRelation &, PointsToRelation &, PointsToRelation *, LivenessSet &, std::set<PointsToNode *>&, GlobalVector &);
    bool getCalledFunctionResult(const CallString &, Function *, std::pair<LivenessSet, PointsToRelation>&);
    std::set<PointsToNode *> getReturnValues(const Function *);
    void runOnFunction(Function *, const CallString &, IntraproceduralPointsTo *, PointsToRelation *, LivenessSet *, SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet *>, 8> &);
    bool runOnFunctionAt(const CallString &, Function *, PointsToRelation *, LivenessSet *);
    void addNotInvalidatedRestricted(PointsToRelation &, PointsToRelation *, CallInst *, LivenessSet *);
    LivenessSet getInvalidatedNodes(PointsToRelation *, CallInst *);
    PointsToData data;
    PointsToNodeFactory factory;
    std::set<PointsToNode *> globals;
    DenseMap<Function *, GlobalVector> GlobalsMap;
};

#endif
