#ifndef LFCPA_INTRAPROCEDURALLIVENESSPOINTSTO_H
#define LFCPA_INTRAPROCEDURALLIVENESSPOINTSTO_H

#include <set>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"

#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

using namespace llvm;

class IntraproceduralLivenessPointsTo {
public:
    typedef std::set<std::pair<PointsToNode *, PointsToNode *>>
        PointsToRelation;
    void runOnFunction (Function&);
    PointsToRelation* getPointsTo (Instruction &) const;
private:
    std::set<PointsToNode *> getRestrictedDef(Instruction *,
                                              PointsToRelation *,
                                              std::set<PointsToNode *> *);
    void insertPointedToBy(std::set<PointsToNode *> &,
                           Value *,
                           PointsToRelation *);
    std::set<PointsToNode *> getPointee(Instruction *, PointsToRelation *);
    void unionCrossProduct(PointsToRelation &,
                           std::set<PointsToNode *> &,
                           std::set<PointsToNode *> &);
    void subtractKill(std::set<PointsToNode *>&,
                      Instruction *,
                      PointsToRelation *);
    void unionRef(std::set<PointsToNode *>&,
                  Instruction *,
                  std::set<PointsToNode *>*,
                  PointsToRelation*);
    void unionRelationRestriction(PointsToRelation &,
                                  PointsToRelation *,
                                  std::set<PointsToNode *> *);
    DenseMap<const Instruction *, PointsToRelation*> pointsto;
    PointsToNodeFactory factory;
};

#endif
