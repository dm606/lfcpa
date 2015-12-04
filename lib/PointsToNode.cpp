#include "../include/PointsToNode.h"

StringRef PointsToNode::getName() const {
    return value->getName();
}
