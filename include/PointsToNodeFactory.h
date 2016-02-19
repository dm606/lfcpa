#ifndef LFCPA_POINTSTONODEFACTORY_H
#define LFCPA_POINTSTONODEFACTORY_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"

#include "PointsToNode.h"

class PointsToNodeFactory {
    private:
        DenseMap<const Value *, PointsToNode *> map;
        DenseMap<const Value *, PointsToNode *> noAliasMap;
        DenseMap<const GlobalVariable *, PointsToNode *> globalMap;
        UnknownPointsToNode unknown;
        bool matchGEPNode(const GEPOperator *, const PointsToNode *) const;
        PointsToNode *getGEPNode(const GEPOperator *, PointsToNode *, PointsToNode *) const;
    public:
        PointsToNode *getUnknown();
        PointsToNode *getNode(const Value *);
        PointsToNode *getNoAliasNode(const AllocaInst *);
        PointsToNode *getNoAliasNode(const CallInst *);
        PointsToNode *getGlobalNode(const GlobalVariable *);
        PointsToNode *getIndexedNode(PointsToNode *, const GEPOperator *);
};

#endif
