#ifndef LFCPA_POINTSTONODE_H
#define LFCPA_POINTSTONODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
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
    PointsToNode() : value(nullptr), stdName("?") {
        name = StringRef(stdName);
    }
    static PointsToNode *createAlloca(AllocaInst *AI) {
        PointsToNode *result = new PointsToNode();
        result->stdName = "alloca:" + AI->getName().str();
        result->name = StringRef(result->stdName);
        return result;
    }
    static PointsToNode *createGlobal(GlobalVariable *AI) {
        PointsToNode *result = new PointsToNode();
        result->stdName = "global:" + AI->getName().str();
        result->name = StringRef(result->stdName);
        return result;
    }
public:
    PointsToNode (Value *value) : value(value) {
        name = value->getName();
        if (name == "") {
            stdName = std::to_string(nextId++);
            name = StringRef(stdName);
        }
    }

    inline bool isGlobal() const {
        return value != nullptr && isa<GlobalValue>(value);
    }

    StringRef getName() const;
};

#endif
