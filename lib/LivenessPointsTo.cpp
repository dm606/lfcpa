#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/LivenessPointsTo.h"
#include "../include/PointsToNode.h"

void LivenessPointsTo::runOnFunction(Function &F) {
    DenseMap<const BasicBlock *, SmallVector<std::pair<PointsToNode, PointsToNode>, 10>> ain, aout;

    for (auto &BB : F) {
        const BasicBlock *b = &BB;
        SmallVector<std::pair<PointsToNode, PointsToNode>, 10> v;
        std::pair<const BasicBlock *, SmallVector<std::pair<PointsToNode, PointsToNode>, 10>> p = std::make_pair(b, v);
        const std::pair<const BasicBlock *, SmallVector<std::pair<PointsToNode, PointsToNode>, 10>> &KV = p;
        ain.insert(KV);
    }

    for (auto &KV : ain) {
        pointsto.insert(KV);
    }
}

SmallVector<std::pair<PointsToNode, PointsToNode>, 10> LivenessPointsTo::getPointsTo(BasicBlock &BB) {
    return pointsto.find(&BB)->second;
}
