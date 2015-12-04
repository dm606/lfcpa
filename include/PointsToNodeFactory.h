#ifndef LFCPA_POINTSTONODEFACTORY_H
#define LFCPA_POINTSTONODEFACTORY_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

#include "PointsToNodeFactory.h"

class PointsToNodeFactory {
    private:
        DenseMap<const Value *, PointsToNode *> map;
    public:
        PointsToNode *getNode(Value *);
};

#endif
