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
    StringRef name;
    std::string stdName;
    PointsToNode() : value(nullptr), stdName("?"), isAlloca(false), isUnknown(true), isPointerType(false) {
        name = StringRef(stdName);
    }
    PointsToNode(std::string stdName, bool isAlloca, bool isPointerType) : value(nullptr), stdName(stdName), isAlloca(isAlloca), isUnknown(false), isPointerType(isPointerType) {
        // Note that stdName.data can be destroyed, so since the StringRef
        // copies a pointer to it, this->stdName *must* be used.
        name = StringRef(this->stdName);
    }
    static PointsToNode *createAlloca(AllocaInst *AI) {
        return new PointsToNode("alloca:" + AI->getName().str(), true, AI->getAllocatedType()->isPointerTy());
    }
    static PointsToNode *createGlobal(GlobalVariable *AI) {
        return new PointsToNode("global:" + AI->getName().str(), false, AI->getValueType()->isPointerTy());
    }

public:
    const bool isAlloca, isUnknown, isPointerType;
    PointsToNode (const Value *value) : value(value), isAlloca(false), isUnknown(false), isPointerType(value->getType()->isPointerTy()) {
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

    inline StringRef getName() const {
        return name;
    }
};

#endif
