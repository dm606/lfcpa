#ifndef LFCPA_LIVENESSPOINTSTO_H
#define LFCPA_LIVENESSPOINTSTO_H

#include <set>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

using namespace llvm;

class LivenessPointsTo {
public:
    void runOnFunction (Function&);
    std::set<std::pair<PointsToNode*, PointsToNode*>>* getPointsTo (Instruction &) const;
private:
    void subtractKill(std::set<PointsToNode *>&, Instruction *);
    void unionRef(std::set<PointsToNode *>&, Instruction *, std::set<PointsToNode *>*, std::set<std::pair<PointsToNode*, PointsToNode*>>*);
    DenseMap<const Instruction *, std::set<std::pair<PointsToNode*, PointsToNode*>>*> pointsto;
    PointsToNodeFactory factory;
};

#endif
