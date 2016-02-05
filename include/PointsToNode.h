#ifndef LFCPA_POINTSTONODE_H
#define LFCPA_POINTSTONODE_H

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class PointsToNode {
public:
    enum PointsToNodeKind {
        PTNK_Unknown,
        PTNK_Value,
        PTNK_Global,
        PTNK_Alloca
    };
private:
    const PointsToNodeKind Kind;
protected:
    StringRef name;
    static int nextId;

    PointsToNode(PointsToNodeKind K) : Kind(K) {}
public:
    PointsToNodeKind getKind() const { return Kind; }

    virtual bool isGlobalAddress() { return false; }
    virtual bool hasPointerType() { return false; }
    virtual bool multipleStackFrames() { return false; }

    inline StringRef getName() const {
        return name;
    }
};


class UnknownPointsToNode : public PointsToNode {
    private:
        std::string stdName;
    public:
        UnknownPointsToNode() : PointsToNode(PTNK_Unknown), stdName("?") {
            name = StringRef(stdName);
        }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Unknown;
        }
};

class ValuePointsToNode : public PointsToNode {
    private:
        std::string stdName;
        const Value *V;
        bool isPointer, globalAddress, userOrArg;
    public:
        ValuePointsToNode(const Value *V) : PointsToNode(PTNK_Value), V(V) {
            assert(V != nullptr);
            name = V->getName();
            if (name == "") {
                stdName = std::to_string(nextId++);
                name = StringRef(stdName);
            }
            globalAddress = isa<GlobalValue>(V);
            isPointer = V->getType()->isPointerTy();
            userOrArg = isa<User>(V) || isa<Argument>(V);
        }

        bool isGlobalAddress() override { return globalAddress; }
        bool hasPointerType() override { return isPointer; }
        bool multipleStackFrames() override { return userOrArg; }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Value;
        }
};

class GlobalPointsToNode : public PointsToNode {
    private:
        std::string stdName;
        bool isPointer;
    public:
        GlobalPointsToNode(GlobalVariable *G) : PointsToNode(PTNK_Global) {
           stdName = "global:" + G->getName().str();
           name = StringRef(stdName);
           isPointer = G->getValueType()->isPointerTy();
        }

        bool hasPointerType() override { return isPointer; }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Global;
        }
};

class AllocaPointsToNode : public PointsToNode {
    private:
        std::string stdName;
        bool isPointer;
    public:
        AllocaPointsToNode(AllocaInst *AI) : PointsToNode(PTNK_Alloca) {
           stdName = "alloca:" + AI->getName().str();
           name = StringRef(stdName);
           isPointer = AI->getAllocatedType()->isPointerTy();
        }

        bool hasPointerType() override { return isPointer; }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Alloca;
        }
};

#endif
