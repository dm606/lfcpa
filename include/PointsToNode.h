#ifndef LFCPA_POINTSTONODE_H
#define LFCPA_POINTSTONODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class PointsToNode {
    friend class PointsToNodeFactory;
private:
    static int nextId;
    const Value *value;
    std::string stdName;
    StringRef name;
    PointsToNode() {
        stdName = "?";
        name = StringRef(stdName);
    }
public:
    PointsToNode (Value *value) : value(value) {
        name = value->getName();
        if (name == "") {
            stdName = std::to_string(nextId++);
            name = StringRef(stdName);
        }
    }

    StringRef getName() const;
};

#endif
