#ifndef LFCPA_LIVENESSPOINTSTO_H
#define LFCPA_LIVENESSPOINTSTO_H

#include <set>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

using namespace llvm;

class LivenessPointsTo {
public:
    typedef std::set<std::pair<PointsToNode *, PointsToNode *>> PointsToSet;
    void runOnFunction (Function&);
    PointsToSet* getPointsTo (Instruction &) const;
private:
    void subtractKill(std::set<PointsToNode *>&, Instruction *, PointsToSet *);
    void unionRef(std::set<PointsToNode *>&,
                  Instruction *,
                  std::set<PointsToNode *>*,
                  PointsToSet*);
    void unionRelationRestriction(PointsToSet &,
                                  PointsToSet *,
                                  std::set<PointsToNode *> *);
    DenseMap<const Instruction *, PointsToSet*> pointsto;
    PointsToNodeFactory factory;
};

#endif
