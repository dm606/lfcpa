#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/PointsToNode.h"
#include "../include/LivenessPointsTo.h"

void LivenessPointsTo::runOnFunction(Function &F) {
    DenseMap<const BasicBlock *, SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10> *> ain, aout;

    // Create vectors to store the points-to information in.
    for (auto &BB : F) {
        ain.insert(std::make_pair(&BB, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
        aout.insert(std::make_pair(&BB, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
    }

    for (auto &BB : F) {
        auto block_in = ain.find(&BB)->second, block_out = aout.find(&BB)->second;
        Value *last = NULL;
        for (auto &I : BB) {
            if (last != NULL) {
                std::pair<PointsToNode*, PointsToNode*> p = std::make_pair(factory.getNode(&I), factory.getNode(last));
                block_in->push_back(p);
            }
            last = &I;
        }
    }

    for (auto &KV : ain) {
        pointsto.insert(KV);
    }
}

SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>* LivenessPointsTo::getPointsTo(BasicBlock &BB) const {
    return pointsto.find(&BB)->second;
}
