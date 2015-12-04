#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/PointsToNode.h"
#include "../include/LivenessPointsTo.h"

void LivenessPointsTo::runOnFunction(Function &F) {
    DenseMap<const Instruction *, SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10> *> ain, aout;

    // Create vectors to store the points-to information in.
    for (auto &BB : F) {
        for (auto &I : BB) {
            ain.insert(std::make_pair(&I, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
            aout.insert(std::make_pair(&I, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
        }
    }

    for (auto &BB : F) {
        Value *last = NULL;
        for (auto &I : BB) {
            auto instruction_in = ain.find(&I)->second, instruction_out = aout.find(&I)->second;
            if (last != NULL) {
                std::pair<PointsToNode*, PointsToNode*> p = std::make_pair(factory.getNode(&I), factory.getNode(last));
                instruction_in->push_back(p);
            }
            last = &I;
        }
    }

    for (auto &KV : ain) {
        pointsto.insert(KV);
    }
}

SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>* LivenessPointsTo::getPointsTo(Instruction &I) const {
    return pointsto.find(&I)->second;
}
