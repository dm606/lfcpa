#ifndef LFCPA_POINTSTONODEFACTORY_H
#define LFCPA_POINTSTONODEFACTORY_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"

#include "PointsToNode.h"

class BuDDyInit {
    public:
        BuDDyInit();
};

class PointsToNodeFactory {
    private:
        // This object must be created before the unknown and init fields are
        // initialized.
        BuDDyInit _buddy;
        DenseMap<const Value *, PointsToNode *> map;
        DenseMap<const Value *, PointsToNode *> noAliasMap;
        DenseMap<const GlobalObject *, PointsToNode *> globalMap;
        UnknownPointsToNode unknown;
        InitPointsToNode init;
        bool matchGEPNode(const GEPOperator *, const PointsToNode *) const;
        PointsToNode *getGEPNode(const GEPOperator *, const Type *Type, PointsToNode *, PointsToNode *) const;
    public:
        PointsToNode *getUnknown();
        PointsToNode *getInit();
        PointsToNode *getNode(const Value *);
        PointsToNode *getNoAliasNode(const AllocaInst *);
        PointsToNode *getNoAliasNode(const CallInst *);
        PointsToNode *getGlobalNode(const GlobalObject *);
        PointsToNode *getIndexedNode(PointsToNode *, const GEPOperator *);
};

#endif
