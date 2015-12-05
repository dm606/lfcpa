#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/PointsToNode.h"
#include "../include/LivenessPointsTo.h"

void LivenessPointsTo::runOnFunction(Function &F) {
    // Points-to information
    DenseMap<const Instruction *, SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10> *> ain, aout;
    // Liveness information
    DenseMap<const Instruction *, SmallVector<PointsToNode*, 10> *> lin, lout;

    // Create vectors to store the points-to information in.
    for (auto &BB : F) {
        for (auto &I : BB) {
            ain.insert(std::make_pair(&I, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
            aout.insert(std::make_pair(&I, new SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>()));
            lin.insert(std::make_pair(&I, new SmallVector<PointsToNode*, 10>()));
            lout.insert(std::make_pair(&I, new SmallVector<PointsToNode*, 10>()));
        }
    }

    for (auto &BB : F) {
        for (auto &I : BB) {
            auto instruction_ain = ain.find(&I)->second, instruction_aout = aout.find(&I)->second;
            auto instruction_lin = lin.find(&I)->second, instruction_lout = lout.find(&I)->second;

        }
    }

    for (auto &KV : ain) {
        pointsto.insert(KV);
    }
}

SmallVector<std::pair<PointsToNode*, PointsToNode*>, 10>* LivenessPointsTo::getPointsTo(Instruction &I) const {
    return pointsto.find(&I)->second;
}
