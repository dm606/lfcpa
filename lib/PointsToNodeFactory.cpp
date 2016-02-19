#include "llvm/IR/Instructions.h"
#include "llvm/IR/Value.h"

#include "PointsToNode.h"
#include "PointsToNodeFactory.h"

PointsToNode *PointsToNodeFactory::getUnknown() {
    return &unknown;
}

bool PointsToNodeFactory::matchGEPNode(const GEPOperator *I, const PointsToNode *N) const {
    if (const GEPPointsToNode *GEPNode = dyn_cast<GEPPointsToNode>(N)) {
        auto GEPNodeI = GEPNode->indices.begin(), GEPNodeE = GEPNode->indices.end();
        for (auto Index = I->idx_begin(), E = I->idx_end(); Index != E; ++Index, ++GEPNodeI) {
            if (GEPNodeI == GEPNodeE)
                return false;

            ConstantInt *Int = cast<ConstantInt>(Index);
            APInt a = Int->getValue(), b = *GEPNodeI;
            // We can't compare a and b directly here because they might have
            // different bitwidths, so we assume that the values fit into 64
            // bits and compare the zero-extended 64-bit values.
            if (a.getZExtValue() != b.getZExtValue())
                return false;
        }

        return GEPNodeI == GEPNodeE;
    }

    return false;
}

PointsToNode *PointsToNodeFactory::getGEPNode(const GEPOperator *I, PointsToNode *Parent, PointsToNode *Pointee) const {
    // We use a special representation of GEPs which can be analysed to
    // implement field-sensitivity. Multiple values can map to the same GEP node
    // (when the GEP has the same pointer operand and indices).  Note that
    // Parent might not be the node corresponding to the pointer operand of the
    // GEP -- it may be the node that it points to.
    for (PointsToNode *Child : Parent->children) {
        if (matchGEPNode(I, Child)) {
            return Child;
        }
    }

    return new GEPPointsToNode(Parent, I, Pointee);
}

PointsToNode* PointsToNodeFactory::getNode(const Value *V) {
    assert(V != nullptr);

    if (isa<UndefValue>(V))
        return &unknown;

    auto KV = map.find(V);
    if (KV != map.end())
        return KV->second;
    else {
        PointsToNode *Node = nullptr;
        if (const GEPOperator *I = dyn_cast<GEPOperator>(V)) {
            if (I->hasAllConstantIndices()) {
                PointsToNode *Parent = getNode(I->getPointerOperand());
                if (!Parent->isSummaryNode()) {
                    PointsToNode *Pointee = Parent->singlePointee() ? getGEPNode(I, Parent->getSinglePointee(), nullptr) : nullptr;
                    Node = getGEPNode(I, Parent, Pointee);
                }
                else {
                    // Since the parent is a summary node, we use the parent to
                    // represent all subnodes.
                    Node = Parent;
                }
            }
            else {
                // If I is a GEP which cannot be analysed field-sensitively, then we
                // return the node correspoinding to the pointer which is being
                // indexed, but since we cannot perform strong updates, treat it as
                // a summary node.
                Node = getNode(I->getPointerOperand());
                Node->markAsSummaryNode();
            }
        }
        else {
            PointsToNode *Pointee = nullptr;
            if (const GlobalVariable *G = dyn_cast<GlobalVariable>(V))
                Pointee = getGlobalNode(G);
            else if (const AllocaInst *AI = dyn_cast<AllocaInst>(V))
                Pointee = getNoAliasNode(AI);
            else if (const CallInst *CI = dyn_cast<CallInst>(V)) {
                if (CI->paramHasAttr(0, Attribute::NoAlias))
                    Pointee = getNoAliasNode(CI);
            }
            Node = new ValuePointsToNode(V, Pointee);
        }

        map.insert(std::make_pair(V, Node));
        return Node;
    }
}

PointsToNode* PointsToNodeFactory::getNoAliasNode(const AllocaInst *I) {
    auto KV = noAliasMap.find(I);
    if (KV != noAliasMap.end())
        return KV->second;
    else {
        PointsToNode *Node = new NoAliasPointsToNode(I);
        noAliasMap.insert(std::make_pair(I, Node));
        return Node;
    }
}

PointsToNode* PointsToNodeFactory::getNoAliasNode(const CallInst *I) {
    auto KV = noAliasMap.find(I);
    if (KV != noAliasMap.end())
        return KV->second;
    else {
        PointsToNode *Node = new NoAliasPointsToNode(I);
        noAliasMap.insert(std::make_pair(I, Node));
        return Node;
    }
}

PointsToNode* PointsToNodeFactory::getGlobalNode(const GlobalVariable *V) {
    auto KV = globalMap.find(V);
    if (KV != globalMap.end())
        return KV->second;
    else {
        PointsToNode *Node = new GlobalPointsToNode(V);
        globalMap.insert(std::make_pair(V, Node));
        return Node;
    }
}

PointsToNode *PointsToNodeFactory::getIndexedNode(PointsToNode *A, const GEPOperator *GEP) {
    assert(GEP->hasAllConstantIndices());
    assert(!A->singlePointee() && "getIndexedNode cannot be used on nodes with a constant pointee.");
    for (PointsToNode *Child : A->children)
        if (matchGEPNode(GEP, Child))
            return Child;

    // We create a new GEP node which has A as its parent.
    return new GEPPointsToNode(A, GEP->getType(), GEP->idx_begin(), GEP->idx_end(), nullptr);
}
