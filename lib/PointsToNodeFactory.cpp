#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "../include/PointsToNode.h"
#include "../include/PointsToNodeFactory.h"

const PointsToNode *PointsToNodeFactory::getUnknown() const {
    return &unknown;
}

PointsToNode* PointsToNodeFactory::getNode(Value *V) {
    Value *Stripped = V->stripPointerCasts();
    auto KV = map.find(Stripped);
    if (KV != map.end())
        return KV->second;
    else {
        PointsToNode *Node = new PointsToNode(V);
        map.insert(std::make_pair(V, Node));
        return Node;
    }
}

