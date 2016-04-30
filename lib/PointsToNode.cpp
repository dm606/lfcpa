#include "PointsToNode.h"

int PointsToNode::nextId = 0;
int PointsToNode::nextVariableNumber = 0;
int PointsToNode::maxVariables = 32;
SmallVector<PointsToNode *, 1024> PointsToNode::AllNodes;
bdd PointsToNode::SummaryNodesBDD;
bddPair *PointsToNode::LeftToIntermediate;
bddPair *PointsToNode::RightToIntermediate;
bddPair *PointsToNode::RightToLeft;