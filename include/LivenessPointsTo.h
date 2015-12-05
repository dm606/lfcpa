#ifndef LFCPA_LIVENESSPOINTSTO_H
#define LFCPA_LIVENESSPOINTSTO_H

#include "llvm/ADT/SparseSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

using namespace llvm;

class LivenessPointsTo {
public:
    void runOnFunction (Function&);
    SparseSet<std::pair<PointsToNode*, PointsToNode*>>* getPointsTo (Instruction &) const;
private:
    DenseMap<const Instruction *, SparseSet<std::pair<PointsToNode*, PointsToNode*>>*> pointsto;
    PointsToNodeFactory factory;
};

#endif
