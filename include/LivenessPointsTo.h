#ifndef LFCPA_LIVENESSPOINTSTO_H
#define LFCPA_LIVENESSPOINTSTO_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToNode.h"

using namespace llvm;

class LivenessPointsTo {
public:
    void runOnFunction (Function&);
    SmallVector<std::pair<PointsToNode, PointsToNode>, 10>& getPointsTo (BasicBlock &BB);
private:
    DenseMap<BasicBlock*, SmallVector<std::pair<PointsToNode, PointsToNode>, 10>> pointsto;
};

#endif
