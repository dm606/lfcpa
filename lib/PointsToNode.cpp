#include "../include/PointsToNode.h"

int PointsToNode::nextId = 0;

StringRef PointsToNode::getName() const {
    return name;
}
