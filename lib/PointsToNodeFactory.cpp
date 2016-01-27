#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "../include/PointsToNode.h"
#include "../include/PointsToNodeFactory.h"

PointsToNode *PointsToNodeFactory::getUnknown() {
    return &unknown;
}

PointsToNode* PointsToNodeFactory::getNode(Value *V) {
    assert(V != nullptr);
    Value *Stripped = V->stripPointerCasts();
    auto KV = map.find(Stripped);
    if (KV != map.end())
        return KV->second;
    else {
        PointsToNode *Node = new PointsToNode(V);
        map.insert(std::make_pair(Stripped, Node));
        return Node;
    }
}

PointsToNode* PointsToNodeFactory::getAllocaNode(AllocaInst *I) {
    auto KV = allocaMap.find(I);
    if (KV != allocaMap.end())
        return KV->second;
    else {
        PointsToNode *Node = PointsToNode::createAlloca(I);
        allocaMap.insert(std::make_pair(I, Node));
        return Node;
    }
}

PointsToNode* PointsToNodeFactory::getGlobalNode(GlobalVariable *V) {
    auto KV = globalMap.find(V);
    if (KV != globalMap.end())
        return KV->second;
    else {
        PointsToNode *Node = PointsToNode::createGlobal(V);
        globalMap.insert(std::make_pair(V, Node));
        return Node;
    }
}

