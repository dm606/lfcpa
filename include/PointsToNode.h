#ifndef LFCPA_POINTSTONODE_H
#define LFCPA_POINTSTONODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Value.h"

using namespace llvm;

class PointsToNode {
private:
    const Value *value;
public:
    PointsToNode (Value *value) : value(value) {}

    StringRef getName() const;
};

#endif
