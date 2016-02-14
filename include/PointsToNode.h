#ifndef LFCPA_POINTSTONODE_H
#define LFCPA_POINTSTONODE_H

#include <sstream>

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class PointsToNode {
public:
    enum PointsToNodeKind {
        PTNK_Unknown,
        PTNK_Value,
        PTNK_Global,
        PTNK_Alloca,
        PTNK_GEP
    };
    friend class GEPPointsToNode;
    friend class PointsToNodeFactory;
    friend class LivenessSet;
    friend class LivenessPointsTo;
private:
    const PointsToNodeKind Kind;
protected:
    SmallVector<PointsToNode *, 4> children;
    StringRef name;
    static int nextId;
    bool summaryNode = false;

    PointsToNode(PointsToNodeKind K) : Kind(K) {}
public:
    PointsToNodeKind getKind() const { return Kind; }

    virtual bool isGlobalAddress() const { return false; }
    virtual bool hasPointerType() const { return false; }
    virtual bool multipleStackFrames() const { return false; }
    virtual bool isAlloca() const { return false; }
    virtual bool singlePointee() const { return false; }
    virtual PointsToNode *getSinglePointee() const {
        llvm_unreachable("This node doesn't always have a single pointee.");
    }
    inline void markAsSummaryNode() {
        summaryNode = true;
    }
    virtual bool isSummaryNode() const {
        return summaryNode;
    }
    inline StringRef getName() const {
        return name;
    }
    inline bool isSubNodeOf(PointsToNode *N) {
        if (this == N)
            return true;

        for (PointsToNode *Child : N->children)
            if (isSubNodeOf(Child))
                return true;

        return false;
    }
    virtual std::pair<const PointsToNode *, SmallVector<uint64_t, 4>> getAddress() const {
        return std::make_pair(this, SmallVector<uint64_t, 4>());
    }
};


class UnknownPointsToNode : public PointsToNode {
    private:
        std::string stdName;
    public:
        UnknownPointsToNode() : PointsToNode(PTNK_Unknown), stdName("?") {
            name = StringRef(stdName);
        }

        bool isSummaryNode() const override {
            // Unknown nodes are never considered to be summary nodes.
            return false;
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
        PointsToNode *Pointee;
    public:
        ValuePointsToNode(const Value *V, PointsToNode *Pointee) : PointsToNode(PTNK_Value), V(V), Pointee(Pointee) {
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

        ValuePointsToNode(const Value *V) : ValuePointsToNode(V, nullptr) {}

        bool isGlobalAddress() const override { return globalAddress; }
        bool hasPointerType() const override { return isPointer; }
        bool multipleStackFrames() const override { return userOrArg; }
        bool singlePointee() const override { return Pointee != nullptr; }
        PointsToNode *getSinglePointee() const override {
            assert(Pointee != nullptr);
            return Pointee;
        }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Value;
        }
};

class GlobalPointsToNode : public PointsToNode {
    private:
        std::string stdName;
        bool isPointer;
    public:
        GlobalPointsToNode(const GlobalVariable *G) : PointsToNode(PTNK_Global) {
           stdName = "global:" + G->getName().str();
           name = StringRef(stdName);
           isPointer = G->getValueType()->isPointerTy();
        }

        bool hasPointerType() const override { return isPointer; }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Global;
        }
};

class AllocaPointsToNode : public PointsToNode {
    private:
        std::string stdName;
        bool isPointer;
    public:
        AllocaPointsToNode(const AllocaInst *AI) : PointsToNode(PTNK_Alloca) {
           stdName = "alloca:" + AI->getName().str();
           name = StringRef(stdName);
           isPointer = AI->getAllocatedType()->isPointerTy();
        }

        bool hasPointerType() const override { return isPointer; }
        bool multipleStackFrames() const override { return true; }
        bool isAlloca() const override { return true; }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_Alloca;
        }
};

class GEPPointsToNode : public PointsToNode {
    private:
        const PointsToNode *Parent;
        bool pointerType;
        SmallVector<APInt, 8> indices;
        std::string stdName;
        friend class PointsToNodeFactory;
    public:
        GEPPointsToNode(PointsToNode *Parent, const Type *Type, User::const_op_iterator I, User::const_op_iterator E) : PointsToNode(PTNK_GEP), Parent(Parent) {
            pointerType = Type->isPointerTy();

            std::stringstream ns;
            ns << Parent->getName().str();
            for (; I != E; ++I) {
                assert(isa<ConstantInt>(I) && "Can only treat GEPs with constant indices field-sensitively.");
                ConstantInt *Int = cast<ConstantInt>(I);
                indices.push_back(Int->getValue());
                ns << "[";
                ns << Int->getZExtValue();
                ns << "]";
            }
            Parent->children.push_back(this);
            stdName = ns.str();
            name = StringRef(stdName);
        }

        GEPPointsToNode(PointsToNode *Parent, const GEPOperator *V) : GEPPointsToNode(Parent, V->getType(), V->idx_begin(), V->idx_end()) { }

        bool hasPointerType() const override { return pointerType; }
        bool multipleStackFrames() const override { return true; }
        bool isAlloca() const override { return Parent->isAlloca(); }

        static bool classof(const PointsToNode *N) {
            return N->getKind() == PTNK_GEP;
        }

        std::pair<const PointsToNode *, SmallVector<uint64_t, 4>> getAddress() const override {
            // We remove all of the zeros from the end of the list of indices
            // because they do not affect the address.
            int stagedZeros = 0;
            SmallVector<uint64_t, 4> resultIndices;
            for (const APInt &I : indices) {
                if (I == 0)
                    stagedZeros++;
                else {
                    while (stagedZeros > 0) {
                        resultIndices.push_back(0);
                        --stagedZeros;
                    }
                    resultIndices.push_back(I.getZExtValue());
                }
            }
            return std::make_pair(Parent, resultIndices);
        }
};

#endif
