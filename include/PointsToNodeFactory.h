#ifndef LFCPA_POINTSTONODEFACTORY_H
#define LFCPA_POINTSTONODEFACTORY_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

#include "PointsToNode.h"

class PointsToNodeFactory {
    private:
        DenseMap<const Value *, PointsToNode *> map;
        DenseMap<const AllocaInst *, PointsToNode *> allocaMap;
        DenseMap<const GlobalVariable *, PointsToNode *> globalMap;
        PointsToNode unknown;
    public:
        PointsToNode *getUnknown();
        PointsToNode *getNode(const Value *);
        PointsToNode *getAllocaNode(AllocaInst *);
        PointsToNode *getGlobalNode(GlobalVariable *);
};

#endif
