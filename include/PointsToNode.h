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
    PointsToNode() : value(nullptr), stdName("?"), isAlloca(false) {
        name = StringRef(stdName);
    }
    PointsToNode(bool isAlloca) : isAlloca(isAlloca) {}
    PointsToNode(std::string stdName, bool isAlloca) : value(nullptr), stdName(stdName), isAlloca(isAlloca) {
        name = StringRef(stdName);
    }
    static PointsToNode *createAlloca(AllocaInst *AI) {
        PointsToNode *result = new PointsToNode(true);
        result->stdName = "alloca:" + AI->getName().str();
        result->name = StringRef(result->stdName);
        return result;
    }
    static PointsToNode *createGlobal(GlobalVariable *AI) {
        PointsToNode *result = new PointsToNode(false);
        result->stdName = "global:" + AI->getName().str();
        result->name = StringRef(result->stdName);
        return result;
    }
public:
    const bool isAlloca;
    PointsToNode (Value *value) : value(value), isAlloca(false) {
        name = value->getName();
        if (name == "") {
            stdName = std::to_string(nextId++);
            name = StringRef(stdName);
        }
    }

    inline bool isGlobal() const {
        return value != nullptr && isa<GlobalValue>(value);
    }

    inline bool multipleStackFrames() const {
        return value != nullptr && (isa<User>(value) || isa<Argument>(value));
    }

    StringRef getName() const;
};

#endif
