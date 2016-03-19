#include <set>

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "LivenessPointsToMisc.h"
#include "LivenessPointsTo.h"
#include "PointsToData.h"
#include "PointsToNode.h"

// Some statistics (LLVM_STATISTIC doesn't work out of tree)
unsigned LivenessPointsTo::worklistIterations = 0;
unsigned LivenessPointsTo::timesRanOnFunction = 0;

bool createdSummaryNode = false;

typedef SmallVector<APInt, 8> IndexList;

std::set<PointsToNode *> LivenessPointsTo::getPointsToSet(const Value *V, bool &AllowMustAlias) {
    if (const Instruction *I = dyn_cast<Instruction>(V)) {
        PointsToNode *N = factory.getNode(I);
        // If N is a summary node, the data may include pointees of fields.
        if (N->isAlwaysSummaryNode() || !N->isFieldSensitive())
            AllowMustAlias = false;
        const BasicBlock *BB = I->getParent();
        const Function *F = BB->getParent();
        ProcedurePointsTo *P = data.getAtFunction(F);
        for (auto p : *P) {
            if (std::get<0>(p) == CallString::empty()) {
                auto P = std::get<1>(p)->find(I);
                if (P == std::get<1>(p)->end())
                    return std::set<PointsToNode *>();
                PointsToRelation *R = P->second.second;
                std::set<PointsToNode *> s;
                for (auto Pointee = R->pointee_begin(N), E = R->pointee_end(N); Pointee != E; ++Pointee)
                    s.insert(*Pointee);
                return s;
            }
        }

        llvm_unreachable("Couldn't find any points-to data for V.");
    }
    else if (const GlobalVariable *G = dyn_cast<GlobalVariable>(V)) {
        std::set<PointsToNode *> s;
        s.insert(factory.getGlobalNode(G));
        return s;
    }
    else if (const GEPOperator *GEP = dyn_cast<GEPOperator>(V)) {
        if (GEP->hasAllConstantIndices()) {
            if (const GlobalVariable *Base = dyn_cast<GlobalVariable>(GEP->getPointerOperand())) {
                std::set<PointsToNode *> s;
                PointsToNode *Global = factory.getGlobalNode(Base);
                s.insert(factory.getIndexedNode(Global, GEP));
                return s;
            }
        }
        else {
            std::set<PointsToNode *> s;
            // We represent non-constant GEPs by the node corresponding to the
            // pointer operand. Note that we cannot use this result as the basis
            // of a PartialAlias or MustAlias result.
            s.insert(factory.getNode(GEP->getPointerOperand()));
            AllowMustAlias = false;
            return s;
        }
    }

    // If we can't determine what V can point to, return the empty set (i.e.
    // "don't know").
    return std::set<PointsToNode *>();
}

std::pair<PointsToNode *, PointsToNode *> makePointsToPair(PointsToNode *Pointer, PointsToNode *Pointee) {
    if (Pointer->pointeesAreSummaryNodes() && !Pointee->isAlwaysSummaryNode()) {
        // If we turn the pointee into a summary node, this may affect what
        // stores to the pointee do. However, these will be added to the
        // worklist again.
        createdSummaryNode = true;
        Pointee->markAsSummaryNode();
    }

    return {Pointer, Pointee};
}

bool isLive(PointsToNode *N, LivenessSet *L) {
    return N->singlePointee() || !N->hasPointerType() || L->find(N) != L->end();
}

bool isDescendantLive(PointsToNode *N, LivenessSet *L) {
    if (isLive(N, L))
        return true;

    if (N->isAggregate())
        for (PointsToNode *C : N->children)
            if (isDescendantLive(C, L))
                return true;

    return false;
}

void makeDescendantsAndPointeesLive(LivenessSet &Lin, PointsToNode *N, PointsToRelation *Ain) {
    Lin.insert(N);
    for (auto P = Ain->pointee_begin(N), E = Ain->pointee_end(N); P != E; ++P)
        Lin.insert(*P);
    for (auto *Child : N->children)
        makeDescendantsAndPointeesLive(Lin, Child, Ain);
}

void addDescendants(SmallVector<std::pair<IndexList, PointsToNode *>, 8> &V, PointsToNode *N, IndexList &I) {
    V.push_back({I, N});

    for (auto *Child : N->children) {
        assert(isa<GEPPointsToNode>(Child));
        GEPPointsToNode *C = cast<GEPPointsToNode>(Child);
        IndexList newList = I;
        newList.append(C->indices.begin(), C->indices.end());
        addDescendants(V, C, newList);
    }
}

SmallVector<std::pair<IndexList, PointsToNode *>, 8> getDescendants(PointsToNode *N) {
    SmallVector<std::pair<IndexList, PointsToNode *>, 8> result;
    IndexList i;
    addDescendants(result, N, i);
    return result;
}

enum IndexListMatch { NoMatch, Shorter, Exact, Longer };

IndexListMatch matchIndexLists(IndexList &A, IndexList &B) {
    auto AI = A.begin(), AE = A.end();
    auto BI = B.begin(), BE = B.end();
    while (AI != AE && BI != BE) {
        if (AI->getZExtValue() != BI->getZExtValue())
            return NoMatch;
        ++AI;
        ++BI;
    }

    if (AI == AE && BI != BE)
        return Shorter;
    else if (AI != AE && BI == BE)
        return Longer;
    else {
        assert(AI == AE && BI == BE);
        return Exact;
    }
}

bool prefixesMatch(IndexList &A, IndexList &B) {
    auto AI = A.begin(), AE = A.end();
    auto BI = B.begin(), BE = B.end();
    while (AI != AE && BI != BE) {
        if (AI->getZExtValue() != BI->getZExtValue())
            return false;
        ++AI;
        ++BI;
    }
    return true;
}

void killDescendants(LivenessSet &Lin, PointsToNode *N) {
    Lin.erase(N);

    for (PointsToNode *C : N->children)
        Lin.erase(C);
}

void subtractKillStoreInst(const CallString &CS, LivenessSet &Lin, PointsToNode *Ptr, PointsToRelation *Ain) {
    if (!Ptr->isAggregate()) {
        bool strongUpdate = true;
        PointsToNode *PointedTo = nullptr;

        for (auto P = Ain->pointee_begin(Ptr), E = Ain->pointee_end(Ptr); P != E; ++P) {
            if ((*P)->isSummaryNode(CS) || PointedTo != nullptr) {
                strongUpdate = false;
                break;
            }
            else
                PointedTo = *P;
        }

        if (strongUpdate) {
            if (PointedTo == nullptr || isa<UnknownPointsToNode>(PointedTo)) {
                // We have no information about what Ptr can point to, so kill
                // everything (except summary nodes).
                Lin.eraseNonSummaryNodes(CS);
            }
            else {
                // Ptr must point to PointedTo, so we can do a strong update
                // here.
                Lin.erase(PointedTo);
            }
        }
        else {
            // If there is more than one value that is possibly pointed to by Ptr,
            // then we need to perform a weak update, so we don't kill anything
            // else.
        }
    }
    else {
        // If the node is an aggregate, we treat the store as a store to each of
        // its children.
        for (PointsToNode *N : Ptr->children)
            subtractKillStoreInst(CS, Lin, N, Ain);
    }
}

void LivenessPointsTo::subtractKill(const CallString &CS,
                                    LivenessSet &Lin,
                                    Instruction *I,
                                    PointsToRelation *Ain) {
    assert(!isa<CallInst>(I) && "CallInsts are analysed using a different part of the code.");
    PointsToNode *N = factory.getNode(I);

    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *Ptr = SI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);
        subtractKillStoreInst(CS, Lin, PtrNode, Ain);
    }
    else if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
        PointsToNode *Alloca = factory.getNoAliasNode(AI);
        if (!Alloca->isSummaryNode(CS))
            killDescendants(Lin, Alloca);
    }
    else if (isa<GetElementPtrInst>(I) || isa<PHINode>(I) || isa<SelectInst>(I) || isa<ReturnInst>(I) || isa<UnreachableInst>(I) || isa<LoadInst>(I)) {
        // These instructions only kill themselves.
    }
    else {
        // All instructions kill themselves, but always killing the instruction
        // can lead to incorrect results because not all instructions are
        // supported: killing them will result in them having no pointees and
        // therefore cause problems when two edges flow into the same basic
        // block.
        // Instead we simply don't kill them. This means that they will be live
        // at the beginning of the function being analysed, and therefore will
        // be made to point to ?.
        return;
    }

    // Note that we don't kill GEPs where they are defined; instead, we kill
    // them where the parent is defined, so that their points-to information is
    // preserved for longer.
    if (!N->isSummaryNode(CS) && !isa<GetElementPtrInst>(I))
        killDescendants(Lin, N);
}

void makeDescendantsLive(LivenessSet &Lin, PointsToNode *N) {
    Lin.insert(N);

    for (PointsToNode *D : N->children)
        makeDescendantsLive(Lin, D);
}

void makeDescendantsPointTo(PointsToRelation &Aout, PointsToNode *N, PointsToNode *Pointee, LivenessSet *Lout) {
    if (Lout->find(N) != Lout->end())
        Aout.insert(makePointsToPair(N, Pointee));

    for (PointsToNode *D : N->children)
        makeDescendantsPointTo(Aout, D, Pointee, Lout);
}

bool isPointeeLive(PointsToNode *N, LivenessSet *Lout, PointsToRelation *Ain) {
    for (auto P = Ain->pointee_begin(N), E = Ain->pointee_end(N); P != E; ++P)
        if (isLive(*P, Lout))
            return true;

    return false;
}

bool isPointeeOfDescendantLive(PointsToNode *N, LivenessSet *Lout, PointsToRelation *Ain) {
    if (isPointeeLive(N, Lout, Ain))
        return true;

    for (PointsToNode *C : N->children)
        if (isPointeeOfDescendantLive(C, Lout, Ain))
            return true;

    return false;
}

PointsToNode *findDescendantExact(PointsToNode *N, IndexList &L) {
    for (auto D : getDescendants(N))
        if (matchIndexLists(D.first, L) == Exact)
            return D.second;

    return nullptr;
}

void makeChildren(PointsToNode *NoChildren, PointsToNode *SomeChildren) {
    assert(NoChildren->isFieldSensitive());
    assert(SomeChildren->isFieldSensitive());
    for (auto D : getDescendants(SomeChildren)) {
        if (D.first.empty())
            continue;
        assert(isa<GEPPointsToNode>(D.second));
        GEPPointsToNode *N = cast<GEPPointsToNode>(D.second);
        // FIXME: Do these nodes always have pointer type?
        assert(N->NodeType->isPointerTy());
        PointsToNode *Pointee = nullptr;
        if (NoChildren->singlePointee()) {
            Type *T = N->NodeType->getPointerElementType();
            assert(T->isPointerTy());
            Pointee = findDescendantExact(NoChildren->getSinglePointee(), D.first);
            if (Pointee == nullptr)
                Pointee = new GEPPointsToNode(NoChildren->getSinglePointee(), T->getPointerElementType(), D.first, nullptr);
        }
        // The constructor adds the node to the list of children.
        new GEPPointsToNode(NoChildren, N->NodeType->getPointerElementType(), D.first, Pointee);
    }
}

void makeChildrenPointer(PointsToNode *NoChildren, PointsToNode *SomeChildren) {
    assert(NoChildren->isFieldSensitive());
    assert(SomeChildren->isFieldSensitive());
    for (auto D : getDescendants(SomeChildren)) {
        if (D.first.empty())
            continue;
        assert(isa<GEPPointsToNode>(D.second));
        GEPPointsToNode *N = cast<GEPPointsToNode>(D.second);
        PointsToNode *Pointee = nullptr;
        const Type *T = N->NodeType;
        if (NoChildren->singlePointee()) {
            Pointee = findDescendantExact(NoChildren->getSinglePointee(), D.first);
            if (Pointee == nullptr)
                Pointee = new GEPPointsToNode(NoChildren->getSinglePointee(), T, D.first, nullptr);
        }
        // The constructor adds the node to the list of children.
        // I don't like the const cast here, but LLVM before version 3.8.0
        // doesn't mark getPointerTo as const, so its needed.
        new GEPPointsToNode(NoChildren, const_cast<Type*>(T)->getPointerTo(), D.first, Pointee);
    }
}

void unionRefLoadInst(LivenessSet& Lin, PointsToNode *Ptr, PointsToNode *Load, LivenessSet *Lout, PointsToRelation *Ain) {
    if (!Ptr->isAggregate() && isDescendantLive(Load, Lout)) {
        Lin.insert(Ptr);
        for (auto P = Ain->pointee_begin(Ptr), E = Ain->pointee_end(Ptr); P != E; ++P)
            Lin.insert(*P);
    }
    else if (Ptr->isAggregate()) {
        if (!Load->isAggregate()) {
            if (!isa<GEPPointsToNode>(Load) && Load->isFieldSensitive()) {
                // If a node is being treated field sensitively but is not an
                // aggregate node, then (since Ptr here must be an aggregate
                // node), it is because no children have been created for it. We
                // create them here so that pointer information is correctly
                // tracked.
                makeChildren(Load, Ptr);
                assert(Load->isAggregate());
            }
            else {
                if (isLive(Load, Lout))
                    makeDescendantsAndPointeesLive(Lin, Ptr, Ain);
                return;
            }
        }

        auto desc = getDescendants(Ptr);
        for (auto D : getDescendants(Load))
            if (isLive(D.second, Lout))
                for (auto PtrD : desc)
                    if (prefixesMatch(D.first, PtrD.first))
                        makeDescendantsAndPointeesLive(Lin, PtrD.second, Ain);
    }
}

void unionRefStoreInst(LivenessSet &Lin, PointsToNode *Ptr, PointsToNode *Value, LivenessSet *Lout, PointsToRelation *Ain) {
    if (!Ptr->isAggregate() && !Value->isAggregate()) {
        Lin.insert(Ptr);

        // We only consider the stored value to be ref'd if at least one of the
        // values that can be pointed to by Ptr is live.
        if (isPointeeLive(Ptr, Lout, Ain))
            makeDescendantsLive(Lin, Value);
    }
    else {
        if (!Ptr->isAggregate()) {
            if (!isa<GEPPointsToNode>(Ptr) && Ptr->isFieldSensitive()) {
                // If a node is being treated field sensitively but is not an
                // aggregate node, then (since Value here must be an aggregate
                // node), it is because no children have been created for it. We
                // create them here so that pointer information is correctly
                // tracked.
                makeChildrenPointer(Ptr, Value);
                assert(Ptr->isAggregate());
            }
        }

        makeDescendantsLive(Lin, Ptr);

        if (!Value->isAggregate()) {
            if (!isa<GEPPointsToNode>(Value) && Value->isFieldSensitive()) {
                // If a node is being treated field sensitively but is not an
                // aggregate node, then (since Ptr here must be an aggregate
                // node), it is because no children have been created for it. We
                // create them here so that pointer information is correctly
                // tracked.
                makeChildren(Value, Ptr);
                assert(Value->isAggregate());
            }
            else {
                if (isPointeeOfDescendantLive(Ptr, Lout, Ain))
                    Lin.insert(Value);
                return;
            }
        }

        auto desc = getDescendants(Value);
        for (auto D : getDescendants(Ptr))
            if (isPointeeLive(D.second, Lout, Ain))
                for (auto ValueD : desc)
                    if (prefixesMatch(D.first, ValueD.first))
                        makeDescendantsAndPointeesLive(Lin, ValueD.second, Ain);
    }
}

void LivenessPointsTo::unionRef(LivenessSet& Lin,
                                Instruction *I,
                                LivenessSet* Lout,
                                PointsToRelation* Ain) {
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        // We only consider the pointer and the possible values in memory to be
        // ref'd if the load is live.
        Value *Ptr = LI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);
        PointsToNode *N = factory.getNode(I);
        unionRefLoadInst(Lin, PtrNode, N, Lout, Ain);
    }
    else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *Ptr = SI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);
        PointsToNode *Value = factory.getNode(SI->getValueOperand());
        unionRefStoreInst(Lin, PtrNode, Value, Lout, Ain);
    }
    else if (isa<PHINode>(I) || isa<SelectInst>(I)) {
        // We only consider the operands of a PHI node or select instruction to
        // be ref'd if I is live.
        PointsToNode *N = factory.getNode(I);
        if (!N->isAggregate()) {
            if (isLive(N, Lout))
                for (Use &U : I->operands())
                    if (Value *Operand = dyn_cast<Value>(U))
                        makeDescendantsLive(Lin, factory.getNode(Operand));
        }
        else {
            if (isDescendantLive(N, Lout)) {
                auto desc = getDescendants(N);
                for (Use &U : I->operands()) {
                    if (Value *Operand = dyn_cast<Value>(U)) {
                        PointsToNode *OperandNode = factory.getNode(Operand);
                        if (!OperandNode->isAggregate())
                            Lin.insert(OperandNode);
                        else {
                            for (auto D : desc)
                                if (isLive(D.second, Lout))
                                    for (auto OpD : getDescendants(OperandNode))
                                        if (prefixesMatch(D.first, OpD.first))
                                            makeDescendantsLive(Lin, OpD.second);
                        }
                    }
                }
            }
        }
    }
    else {
        // If the instruction is not a load or a store, we consider all of it's
        // operands to be ref'd, even if the instruction is not live.
        for (Use &U : I->operands())
            if (Value *Operand = dyn_cast<Value>(U))
                makeDescendantsLive(Lin, factory.getNode(Operand));
    }
}

void unionDescendants(SmallVector<std::pair<IndexList, PointsToNode *>, 8> &S, const IndexList &L, PointsToNode *N) {
    S.push_back({L, N});

    for (auto *Child : N->children) {
        assert(isa<GEPPointsToNode>(Child));
        GEPPointsToNode *C = cast<GEPPointsToNode>(Child);
        IndexList newList = L;
        newList.append(C->indices.begin(), C->indices.end());
        unionDescendants(S, newList, C);
    }
}

void unionPointeesWithDescendants(SmallVector<std::pair<IndexList, PointsToNode *>, 8> &Pointees, PointsToRelation *Ain, const IndexList &L, PointsToNode *N) {
    if (isa<UnknownPointsToNode>(N)) {
        // Assume here that ?-->?.
        Pointees.push_back({L, N});
        return;
    }

    for (auto P = Ain->pointee_begin(N), E = Ain->pointee_end(N); P != E; ++P)
        unionDescendants(Pointees, L, *P);

    for (auto *Child : N->children) {
        assert(isa<GEPPointsToNode>(Child));
        GEPPointsToNode *C = cast<GEPPointsToNode>(Child);
        IndexList newList = L;
        newList.append(C->indices.begin(), C->indices.end());
        unionPointeesWithDescendants(Pointees, Ain, newList, C);
    }
}

void unionRelationApplicationWithDescendants(SmallVector<std::pair<IndexList, PointsToNode *>, 8> &Pointees, PointsToRelation *Ain, const SmallVector<std::pair<IndexList, PointsToNode *>, 8> &S) {
    for (auto P : S)
        unionPointeesWithDescendants(Pointees, Ain, P.first, P.second);
}

void insertNewPairsLoadInst(PointsToRelation &Aout, PointsToNode *Load, PointsToNode *Ptr, PointsToNode *Unknown, PointsToRelation *Ain, LivenessSet *Lout) {
    if (!Load->isAggregate()) {
        if (!isLive(Load, Lout))
            return;

        // If the pointer is an aggregate node, we don't know what the result of
        // the load points to.
        if (Ptr->isAggregate())
            Aout.insert({Load, Unknown});
        else {
            std::set<PointsToNode *> t;
            for (auto P = Ain->pointee_begin(Ptr), E = Ain->pointee_end(Ptr); P != E; ++P) {
                if (isa<UnknownPointsToNode>(*P))
                    Aout.insert({Load, Unknown});
                t.insert(*P);
            }
            for (auto P = Ain->restriction_begin(t), E = Ain->restriction_end(t); P != E; ++P)
                Aout.insert(makePointsToPair(Load, P->second));
        }
    }
    else {
        SmallVector<std::pair<IndexList, PointsToNode *>, 8> p, pointees;
        IndexList l;
        unionPointeesWithDescendants(pointees, Ain, l, Ptr);
        unionRelationApplicationWithDescendants(p, Ain, pointees);
        for (auto D : getDescendants(Load)) {
            if (isLive(D.second, Lout)) {
                for (auto P : p) {
                    switch (matchIndexLists(D.first, P.first)) {
                        case Exact:
                            Aout.insert(makePointsToPair(D.second, P.second));
                            break;
                        case Shorter:
                            // If D.second is an aggregate points to pairs will
                            // be added for its children.
                            // FIXME: Does the commmented code (or something
                            // similar) need executing?
                            (void)Unknown;
                            /*
                            if (!D.second->isAggregate())
                                Aout.insert({D.second, Unknown});
                            */
                            break;
                        case Longer:
                        case NoMatch:
                            break;
                    }
                }
            }
        }
    }
}

void insertNewPairsStoreInst(PointsToRelation &Aout, PointsToNode *Ptr, PointsToNode *Value, PointsToNode *Unknown, PointsToRelation *Ain, LivenessSet *Lout) {
    bool insertUnknowns = false;
    if (!Ptr->isAggregate() && !Value->isAggregate()) {
        for (auto P = Ain->pointee_begin(Ptr), PE = Ain->pointee_end(Ptr); P != PE; ++P) {
            if (isa<UnknownPointsToNode>(Ptr)) {
                insertUnknowns = true;
                continue;
            }
            if (Lout->find(*P) != Lout->end())
                for (auto Q = Ain->pointee_begin(Value), QE = Ain->pointee_end(Value); Q != QE; ++Q)
                    Aout.insert(makePointsToPair(*P, *Q));
        }
    }
    else {
        SmallVector<std::pair<IndexList, PointsToNode *>, 8> ptrPointees, valuePointees;
        IndexList l;
        unionPointeesWithDescendants(ptrPointees, Ain, l, Ptr);
        unionPointeesWithDescendants(valuePointees, Ain, l, Value);
        for (auto P : ptrPointees) {
            if (isa<UnknownPointsToNode>(P.second)) {
                insertUnknowns = true;
                continue;
            }
            if (Lout->find(P.second) != Lout->end()) {
                for (auto Q : valuePointees) {
                    switch (matchIndexLists(P.first, Q.first)) {
                        case Exact:
                            Aout.insert(makePointsToPair(P.second, Q.second));
                            break;
                        case Shorter:
                            // FIXME: Does the commmented code (or something
                            // similar) need executing?
                            (void)Unknown;
                            /*
                            if (!P.second->isAggregate())
                                Aout.insert({P.second, Unknown});
                            */
                            break;
                        case Longer:
                        case NoMatch:
                            break;
                    }
                }
            }
        }
    }

    if (insertUnknowns) {
        // If Ptr points to ?, then we don't know what anything points to after
        // this instruction. Normally Lout would be empty (everything is
        // killed), but summary nodes cannot be killed.
        for (auto I = Lout->begin(), E = Lout->end(); I != E; ++I)
            Aout.insert({*I, Unknown});
    }
}

void insertNewPairsAssignment(PointsToRelation &Aout, PointsToNode *L, PointsToNode *R, PointsToNode *Unknown, PointsToRelation *Ain, LivenessSet *Lout) {
    if (!L->isAggregate() && !R->isAggregate()) {
        if (Lout->find(L) == Lout->end())
            return;

        for (auto P = Ain->pointee_begin(R), E = Ain->pointee_end(R); P != E; ++P)
            Aout.insert(makePointsToPair(L, *P));
    }
    else {
        SmallVector<std::pair<IndexList, PointsToNode *>, 8> pointees;
        IndexList l;
        unionPointeesWithDescendants(pointees, Ain, l, R);
        for (auto D : getDescendants(L)) {
            if (Lout->find(D.second) != Lout->end()) {
                for (auto P : pointees) {
                    switch (matchIndexLists(D.first, P.first)) {
                        case Exact:
                            Aout.insert(makePointsToPair(D.second, P.second));
                            break;
                        case Shorter:
                            // FIXME: Does the commmented code (or something
                            // similar) need executing?
                            (void)Unknown;
                            /*
                            if (!D.second->isAggregate())
                                Aout.insert({D.second, Unknown});
                            */
                            break;
                        case Longer:
                        case NoMatch:
                            break;
                    }
                }
            }
        }
    }
}

void LivenessPointsTo::insertNewPairs(PointsToRelation &Aout, Instruction *I, PointsToRelation *Ain, LivenessSet *Lout) {
    PointsToNode *Unknown = factory.getUnknown();
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        PointsToNode *Load = factory.getNode(LI);
        PointsToNode *Pointer = factory.getNode(LI->getPointerOperand());
        insertNewPairsLoadInst(Aout, Load, Pointer, Unknown, Ain, Lout);
    }
    else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        PointsToNode *Ptr = factory.getNode(SI->getPointerOperand());
        PointsToNode *Value = factory.getNode(SI->getValueOperand());
        insertNewPairsStoreInst(Aout, Ptr, Value, Unknown, Ain, Lout);
    }
    else if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
        PointsToNode *Select = factory.getNode(SI);
        insertNewPairsAssignment(Aout, Select, factory.getNode(SI->getFalseValue()), Unknown, Ain, Lout);
        insertNewPairsAssignment(Aout, Select, factory.getNode(SI->getTrueValue()), Unknown, Ain, Lout);
    }
    else if (PHINode *Phi = dyn_cast<PHINode>(I)) {
        PointsToNode *N = factory.getNode(Phi);
        for (auto &V : Phi->incoming_values())
            insertNewPairsAssignment(Aout, N, factory.getNode(V), Unknown, Ain, Lout);
    }
    else if (GEPOperator *GEP = dyn_cast<GEPOperator>(I)) {
        PointsToNode *N = factory.getNode(GEP);
        PointsToNode *Ptr = factory.getNode(GEP->getPointerOperand());
        if (N->singlePointee()) {
            // The points to pair for N is implicit, so nothing needs to be added
            // here.
            return;
        }

        if (N->pointeesAreSummaryNodes() || !N->isFieldSensitive()) {
            // The value being indexed is treated insensitively, so we don't need to
            // do anything to Aout here.
            return;
        }

        if (Lout->find(N) == Lout->end())
            return;

        assert(GEP->hasAllConstantIndices());

        for (auto P = Ain->pointee_begin(Ptr), E = Ain->pointee_end(Ptr); P != E; ++P) {
            if (isa<UnknownPointsToNode>(*P) || !(*P)->isFieldSensitive()) {
                // We don't know what the pointer points to or we don't treat the pointee field sensitively, so we don't know what
                // the GEP points to.
                Aout.insert({N, Unknown});
            }
            else
                Aout.insert({N, factory.getIndexedNode(*P, GEP)});
        }
    }
    else if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
        PointsToNode *Alloca = factory.getNoAliasNode(AI);
        makeDescendantsPointTo(Aout, Alloca, Unknown, Lout);
    }
}

ProcedurePointsTo *LivenessPointsTo::getPointsTo(Function &F) const {
    return data.getAtFunction(&F);
}

bool hasPointee(PointsToRelation &S, PointsToNode *N) {
    return S.pointee_begin(N) != S.pointee_end(N);
}

void LivenessPointsTo::computeLout(Instruction *I, LivenessSet* Lout, IntraproceduralPointsTo *Result) {
    if (isa<ReturnInst>(I)) {
        // After a return instruction, nothing is live.
    }
    else if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
        // If this instruction is a terminator, it may have multiple
        // successors.
        Lout->clear();
        for (unsigned i = 0; i < TI->getNumSuccessors(); i++) {
            Instruction *Succ = TI->getSuccessor(i)->begin();
            auto succ_result = Result->find(Succ);
            assert(succ_result != Result->end());
            auto succ_lin = succ_result->second.first;
            Lout->insertAll(*succ_lin);
        }
    }
    else {
        // If this instruction is not a terminator, it has exactly one
        // successor -- the next instruction in the function.
        Instruction *Succ = getNextInstruction(I);
        auto succ_result = Result->find(Succ);
        assert(succ_result != Result->end());
        auto succ_lin = succ_result->second.first;
        if (*succ_lin != *Lout) {
            Lout->clear();
            Lout->insertAll(*succ_lin);
        }
    }
}

bool LivenessPointsTo::computeAin(Instruction *I, Function *F, PointsToRelation *Ain, LivenessSet *Lin, IntraproceduralPointsTo *Result, bool InsertAtFirstInstruction) {
    // Compute ain for the current instruction.
    PointsToRelation s;
    if (I == &*inst_begin(F)) {
        s = *Ain;
        if (InsertAtFirstInstruction) {
            // If this is the first instruction of the function, then apart from
            // the data in entry, we don't know what anything points to. ain
            // already contains the data in entry, so add the remaining pairs.
            // If this function is being analysed as a callee, then don't insert
            // any pairs of the form x-->? because we might find out what x
            // points to later.
            for (PointsToNode *N : *Lin) {
                if (!hasPointee(s, N)) {
                    std::pair<PointsToNode *, PointsToNode *> p =
                        std::make_pair(N, factory.getUnknown());
                    s.insert(p);
                }
            }
        }
    }
    else {
        // If this is not the first instruction, then the points to
        // information from the predecessors can be propagated forwards.
        BasicBlock *BB = I->getParent();
        Instruction *FirstInBB = BB->begin();
        if (FirstInBB == I) {
            for (pred_iterator PI = pred_begin(BB), E = pred_end(BB);
                 PI != E;
                 ++PI) {
                BasicBlock *PredBB = *PI;
                Instruction *Pred = --(PredBB->end());
                auto pred_result = Result->find(Pred);
                assert(pred_result != Result->end());
                PointsToRelation *PredAout = pred_result->second.second;
                s.unionRelationRestriction(PredAout, Lin);
            }
        }
        else {
            Instruction *Pred = getPreviousInstruction(I);
            auto pred_result = Result->find(Pred);
            assert(pred_result != Result->end());
            PointsToRelation *PredAout = pred_result->second.second;
            s.unionRelationRestriction(PredAout, Lin);
        }
    }
    if (s != *Ain) {
        Ain->clear();
        Ain->insertAll(s);
        return true;
    }

    return false;
}

bool LivenessPointsTo::computeLin(const CallString &CS, Instruction *I, PointsToRelation *Ain, LivenessSet *Lin, LivenessSet *Lout) {
    if (CallInst *CI = dyn_cast<CallInst>(I)) {
        PointsToNode *CINode = factory.getNode(CI);
        CallString newCS = CS.addCallSite(I);
        Function *Called = CI->getCalledFunction();

        bool analysable = Called != nullptr && !Called->isDeclaration();
        if (analysable) {
            // The set of values that are returned from the function.
            std::set<PointsToNode *> returnValues = getReturnValues(Called);

            std::pair<LivenessSet, PointsToRelation> calledFunctionResult = getCalledFunctionResult(newCS, Called);
            auto calledFunctionLin = calledFunctionResult.first;
            auto calledFunctionAout = calledFunctionResult.second;

            LivenessSet n = replaceFormalArgumentsWithActual(CS, Called, CI, calledFunctionLin, Lout);
            // The CallInst is killed by itself, and its value cannot be
            // used by itself, so it is not live immediately before the
            // call is executed.
            n.erase(CINode);
            // If the function's return value has the noalias attribute
            // and the noalias node is not a summary node, then it can
            // be killed here.
            if (CI->paramHasAttr(0, Attribute::NoAlias)) {
                PointsToNode *NoAliasNode = factory.getNoAliasNode(CI);
                if (!NoAliasNode->isSummaryNode(CS))
                  n.erase(NoAliasNode);
            }
            for (auto I = Lout->begin(), E = Lout->end(); I != E; ++I) {
                if ((*I)->isSummaryNode(CS)) {
                    // We shouldn't allow the function call to kill this
                    // node.
                    n.insert(*I);
                }
            }

            // The function is live.
            PointsToNode *CalledValue = factory.getNode(CI->getCalledValue());
            makeDescendantsLive(n, CalledValue);

            // If the two sets are the same, then no changes need to be
            // made to lin, so don't do anything here. Otherwise, we
            // need to update lin and add the predecessors of the
            // current instruction to the worklist.
            if (n != *Lin) {
                Lin->clear();
                Lin->insertAll(n);
                return true;
            }
            else
                return false;
        }
        else {
            // We reach this point if we have a declaration or function
            // pointer. Just assume the worst case -- the function may
            // invalidate or use anything that it has access to.

            // Compute lin for the current instruction. This consists of
            // everything that is live after the call, plus the arguments,
            // minus the return value.

            // We assume that the function will use the value of anything that is
            // reachable from it, and will kill anything that is reachable
            // from it.
            YesNoMaybe EverythingReachable = No;
            LivenessSet reachable, killable;
            insertReachableDeclaration(CS, CI, reachable, killable, Ain, EverythingReachable);

            LivenessSet n = *Lout;

            if (EverythingReachable == Yes) {
                // If everything is reachable, then everything might be
                // ref'd, so insert as much as possible into n.
                n.insertAll(*Lin);
                Ain->insertEverythingInto(n);
            }
            else {
                // If only the nodes in reachable are reachable, then only
                // these should be made live. If we don't yet know whether
                // anything will be reachable or not, we must be
                // conservative for monotonicity.
                n.insertAll(reachable);
            }
            // The return value is never live before the call.
            n.erase(CINode);
            // If the function's return value has the noalias attribute
            // and the noalias node is not a summary node, then it can
            // be killed here.
            if (CI->paramHasAttr(0, Attribute::NoAlias)) {
                PointsToNode *NoAliasNode = factory.getNoAliasNode(CI);
                if (!NoAliasNode->isSummaryNode(CS))
                    n.erase(NoAliasNode);
            }

            // The function is live.
            PointsToNode *CalledValue = factory.getNode(CI->getCalledValue());
            makeDescendantsLive(n, CalledValue);

            // If the two sets are the same, then no changes need to be made to lin,
            // so don't do anything here. Otherwise, we need to update lin and add
            // the predecessors of the current instruction to the worklist.
            if (n != *Lin) {
                Lin->clear();
                Lin->insertAll(n);
                return true;
            }
            else
                return false;
        }
    }
    else {
        // Compute lin for the current instruction.
        LivenessSet n;
        n.insertAll(*Lout);
        subtractKill(CS, n, I, Ain);
        unionRef(n, I, Lout, Ain);
        // If the two sets are the same, then no changes need to be made to lin,
        // so don't do anything here. Otherwise, we need to update lin and add
        // the predecessors of the current instruction to the worklist.
        if (n != *Lin) {
            Lin->clear();
            Lin->insertAll(n);
            return true;
        }
        else
            return false;
    }
}

bool LivenessPointsTo::computeAout(const CallString &CS, Instruction *I, PointsToRelation *Ain, PointsToRelation *Aout, LivenessSet *Lout) {
    if (CallInst *CI = dyn_cast<CallInst>(I)) {
        PointsToNode *CINode = factory.getNode(CI);
        CallString newCS = CS.addCallSite(I);
        Function *Called = CI->getCalledFunction();

        bool analysable = Called != nullptr && !Called->isDeclaration();
        if (analysable) {
            // The set of values that are returned from the function.
            std::set<PointsToNode *> returnValues = getReturnValues(Called);

            std::pair<LivenessSet, PointsToRelation> calledFunctionResult = getCalledFunctionResult(newCS, Called);
            auto calledFunctionLin = calledFunctionResult.first;
            auto calledFunctionAout = calledFunctionResult.second;

            PointsToRelation s = replaceReturnValuesWithCallInst(CI, calledFunctionAout, returnValues, Lout);
            for (auto I = Ain->begin(), E = Ain->end(); I != E; ++I) {
                if (I->first->isSummaryNode(CS)) {
                    // We shouldn't allow the function call to remove
                    // this pair. (Actually it is never *removed*, but
                    // it just isn't discovered in recursive functions).
                     s.insert(*I);
                }
            }
            if (s != *Aout) {
                Aout->clear();
                Aout->insertAll(s);
                return true;
            }
            else
                return false;
        }
        else {
            // Compute aout for the current instruction. Anything that can
            // be modified by the function (including the return value
            // unless it has the noalias attribute) must point to unknown;
            // anything else points to the same thing that it does in ain.
            // If a node that has no pointees is reachable, we can't insert
            // pairs of the form x-->? unless we know that x is definitely
            // reachable, because we might discover what the node points to
            // later.
            LivenessSet reachable = LivenessSet();
            LivenessSet killable = LivenessSet();
            YesNoMaybe EverythingReachable = No;
            insertReachableDeclaration(CS, CI, reachable, killable, Ain, EverythingReachable);
            if (!CINode->singlePointee())
                reachable.insert(CINode);
            PointsToRelation s;
            if (EverythingReachable == Yes) {
                for (PointsToNode *N : *Lout)
                    s.insert(std::make_pair(N, factory.getUnknown()));
            }
            else if (EverythingReachable == No) {
                if (Lout->find(CINode) != Lout->end())
                    s.insert(std::make_pair(CINode, factory.getUnknown()));
                for (PointsToNode *N : killable)
                    if (Lout->find(N) != Lout->end())
                        s.insert(std::make_pair(N, factory.getUnknown()));
                for (auto P = Ain->restriction_begin(Lout), E = Ain->restriction_end(Lout); P != E; ++P)
                    if (killable.find(P->first) == killable.end())
                        s.insert(*P);
            }
            // If the function's return value has the noalias attribute
            // then we don't know what the noalias points to.
            if (CI->paramHasAttr(0, Attribute::NoAlias)) {
                PointsToNode *NoAliasNode = factory.getNoAliasNode(CI);
                makeDescendantsPointTo(s, NoAliasNode, factory.getUnknown(), Lout);
            }

            if (s != *Aout) {
                Aout->clear();
                Aout->insertAll(s);
                return true;
            }
            else
                return false;
        }
    }
    else {
        PointsToRelation s;
        // Compute aout for the current instruction.
        LivenessSet notKilled = *Lout;
        subtractKill(CS, notKilled, I, Ain);
        s.unionRelationRestriction(Ain, &notKilled);
        insertNewPairs(s, I, Ain, Lout);
        if (s != *Aout) {
            Aout->clear();
            Aout->insertAll(s);
            return true;
        }
        else
            return false;
    }
}

void LivenessPointsTo::insertReachableDeclaration(const CallString &CS, CallInst *CI, LivenessSet &Reachable, LivenessSet &Killable, PointsToRelation *Ain, YesNoMaybe &EverythingReachable) {
    std::set<PointsToNode *> seen;
    // FIXME: Can globals be accessed by the function?
    // This is roughly the mark phase from mark-and-sweep garbage collection. We
    // begin with the roots, which are the arguments of the function,  then
    // determine what is reachable using the points-to relation.
    std::function<void(PointsToNode *)> insertReachable = [&](PointsToNode *N) {
        if (seen.insert(N).second) {
            Reachable.insert(N);
            bool noPointees = true;
            for (auto P = Ain->pointee_begin(N), E = Ain->pointee_end(N); P != E; ++P) {
                if (isa<UnknownPointsToNode>(*P)) {
                    EverythingReachable = Yes;
                    return;
                }
                noPointees = false;
                // A node can only be killed if it is pointed to by something
                // that is reachable, or is a child of something that is
                // killable.
                Killable.insert(*P);
                for (auto *Child : (*P)->children)
                    Killable.insert(Child);
                insertReachable(*P);
            }
            if (noPointees && (N->hasPointerType() || N->isSummaryNode(CS)) && !N->singlePointee() && EverythingReachable == No)
                EverythingReachable = Maybe;

            // If a node is reachable, then so are its subnodes.
            for (auto *Child : N->children)
                insertReachable(Child);
        }
    };

    // Arguments are roots.
    for (Value *V : CI->arg_operands())
        insertReachable(factory.getNode(V));
}

std::pair<LivenessSet, PointsToRelation> LivenessPointsTo::getCalledFunctionResult(const CallString &CS, Function *F) {
    std::pair<LivenessSet, PointsToRelation> Result;
    // If there is an exact match for F and CS in data, then this should be used.
    // Otherwise, we should use the data that is associated with the function
    // and longest possible prefix of the call string. If there is no data for F
    // at all, just return false.

    if (!data.hasDataForFunction(F))
        return Result;

    IntraproceduralPointsTo *PT = data.getAtLongestPrefix(F, CS);
    if (PT == nullptr)
        return Result;
    auto FirstInst = inst_begin(F);
    assert(FirstInst != inst_end(F));
    auto I = PT->find(&*FirstInst);
    assert(I != PT->end());
    Result.first = *I->second.first;

    // For Aout, we need to union over all of the PointsToRelations associated
    // with ReturnInsts.
    PointsToRelation aout;
    for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        Instruction *Inst = &*I;
        if (isa<ReturnInst>(Inst)) {
            auto J = PT->find(Inst);
            assert(J != PT->end());
            aout.insertAll(*J->second.second);
        }
    }
    Result.second = aout;

    return Result;
}

std::set<PointsToNode *> LivenessPointsTo::getReturnValues(const Function *F) {
    std::set<PointsToNode *> s;
    for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I)
    {
        const Instruction* Inst = &*I;
        if (const ReturnInst *RI = dyn_cast<ReturnInst>(Inst))
            if (RI->getReturnValue() != nullptr)
                s.insert(factory.getNode(RI->getReturnValue()));
    }
    return s;
}


LivenessSet LivenessPointsTo::computeFunctionExitLiveness(CallInst *CI, LivenessSet *Lout) {
    PointsToNode *CINode = factory.getNode(CI);

    LivenessSet L;
    for (PointsToNode *N : *Lout) {
        if (N == CINode) {
            // Return values will be made live in the correct places when
            // analysing the function if necessary.
        }
        else
            L.insert(N);
    }

    // The values of the arguments are not live at the end of the function
    // because they cannot be modified by the function; the are inserted into
    // Lin from RemovedActualArguments later.
    // FIXME: What about varargs?
    for (Value *V : CI->arg_operands()) {
        PointsToNode *ArgNode = factory.getNode(V);
        if (L.find(ArgNode) != L.end())
            L.erase(ArgNode);
    }

    return L;
}

PointsToRelation *LivenessPointsTo::replaceActualArgumentsWithFormal(Function *Callee, CallInst *CI, PointsToRelation *Ain) {
    SmallVector<std::pair<PointsToNode *, PointsToNode *>, 8> ArgMap;
    auto Arg = Callee->arg_begin();
    PointsToRelation *R = new PointsToRelation();
    for (Value *V : CI->arg_operands()) {
        PointsToNode *Node = factory.getNode(V);
        // FIXME: What about varargs functions?
        assert(Arg != Callee->arg_end() && "Argument count mismatch");
        Argument *A = &*Arg;
        PointsToNode *ANode = factory.getNode(A);

        ArgMap.push_back({Node, ANode});
        if (Node->singlePointee()) {
            // These nodes won't be seen in the next loop, so insert the correct
            // pairs for them into Ain here.
            R->insert(makePointsToPair(ANode, Node->getSinglePointee()));
        }

        ++Arg;
    }

    for (auto I = Ain->begin(), E = Ain->end(); I != E; ++I) {
        auto MapE = ArgMap.end();
        auto MapI = std::find_if(ArgMap.begin(), MapE, [&](std::pair<PointsToNode *, PointsToNode *> P) {
            return P.first == I->first;
        });
        if (MapI != MapE)
            R->insert(makePointsToPair(MapI->second, I->second));
        else
            R->insert(*I);
    }

    return R;
}

LivenessSet LivenessPointsTo::replaceFormalArgumentsWithActual(const CallString &CS, Function *Callee, CallInst *CI, LivenessSet &CalledFunctionLin, LivenessSet *Lout) {
    LivenessSet L;
    bool calleeInCallString = CI->getParent()->getParent() == Callee || CS.containsCallIn(Callee);

    for (PointsToNode *N : CalledFunctionLin) {
        if (NoAliasPointsToNode *NN = dyn_cast<NoAliasPointsToNode>(N)) {
            if (!calleeInCallString && NN->Definer == Callee) {
                // If the node is defined in Callee and it is a summary node because it may exist in
                // multiple stack frames, then it may be live at the beginning
                // of the function. However, if there is no stack frame in which
                // it exists, then it is safe to kill it.
                continue;
            }
        }

        L.insert(N);
    }

    // Replace formal arguments with actual arguments.
    auto Arg = Callee->arg_begin();
    for (Value *V : CI->arg_operands()) {
        PointsToNode *Node = factory.getNode(V);
        // FIXME: What about varargs functions?
        assert(Arg != Callee->arg_end() && "Argument count mismatch");
        Argument *A = &*Arg;
        PointsToNode *ANode = factory.getNode(A);

        if (L.find(ANode) != L.end()) {
            L.erase(ANode);
            L.insert(Node);
        }

        ++Arg;
    }

    // The values of the arguments are not live at the end of the function
    // because they cannot be modified by the function; they are inserted into
    // Lin directly here.
    // FIXME: What about varargs?
    for (Value *V : CI->arg_operands()) {
        PointsToNode *ArgNode = factory.getNode(V);
        if (Lout->find(ArgNode) != Lout->end())
            L.insert(ArgNode);
    }

    return L;
}

PointsToRelation LivenessPointsTo::replaceReturnValuesWithCallInst(CallInst *CI, PointsToRelation &Aout, std::set<PointsToNode *> &ReturnValues, LivenessSet *Lout) {
    PointsToNode *CINode = factory.getNode(CI);
    bool CINodeLive = Lout->find(CINode) != Lout->end();
    PointsToRelation R;
    for (auto I = Aout.begin(), E  = Aout.end(); I != E; ++I) {
        if (ReturnValues.find(I->first) != ReturnValues.end()) {
            if (CINodeLive)
                R.insert(makePointsToPair(CINode, I->second));
        }
        else if (Lout->find(I->first) != Lout->end())
            R.insert(*I);
    }
    if (CINodeLive) {
        for (PointsToNode *N : ReturnValues) {
            if (N->singlePointee()) {
                // These nodes will not be seen in the previous loop.
                R.insert(makePointsToPair(CINode, N->getSinglePointee()));
            }
        }
    }
    return R;
}

void LivenessPointsTo::runOnFunction(Function *F, const CallString &CS, IntraproceduralPointsTo *Result, PointsToRelation *EntryPointsTo, LivenessSet &ExitLiveness, bool MakeReturnValuesLive, SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet, bool>, 8> &Calls) {
    timesRanOnFunction++;
    assert(EntryPointsTo != nullptr);
    assert(!F->isDeclaration() && "Can only run on definitions.");

    // The result of the function is lin and aout (since liveness is propagated
    // backwards and points-to forwards); this variable contains lout and ain.
    IntraproceduralPointsTo nonresult;

    // Initialize ain, aout, lin and lout for each instruction, and ensure that
    // GEPs are handled correctly.
    for (inst_iterator S = inst_begin(F), I = S, E = inst_end(F); I != E; ++I) {
        Instruction *inst = &*I;

        // If the instruction is a ReturnInst, the values that are live after
        // the instruction is executed are exactly those specified in
        // ExitLiveness, if it exists. If the instruction is the first in the
        // function, the points-to information before it is executed is exactly
        // that in EntryPointsTo.
        if (ReturnInst *RI = dyn_cast<ReturnInst>(inst)) {
            LivenessSet *L = new LivenessSet();
            L->insertAll(ExitLiveness);
            if (RI->getReturnValue() != nullptr && MakeReturnValuesLive)
                L->insert(factory.getNode(RI->getReturnValue()));

            if (I == S)
                nonresult.insert({inst, {L, EntryPointsTo}});
            else
                nonresult.insert({inst, {L, new PointsToRelation()}});
        }
        else if (I == S)
            nonresult.insert({inst, {new LivenessSet(), EntryPointsTo}});
        else
            nonresult.insert({inst, {new LivenessSet(), new PointsToRelation()}});

        if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(inst)) {
            // If some GEPs which are based on a pointer have all constant
            // indices and some have none-constant indices, then we want to
            // treat all of the GEPs based on that pointer field-insensitively.
            // To ensure that this happens, we ensure that a summary node for
            // the pointer is created before any of the GEPs with constant
            // indices are looked at.
            if (!GEP->hasAllConstantIndices())
                factory.getNode(GEP);
        }
    }

    // Create and initialize worklist. Also initialize the values of Lout and
    // Ain, since they are not preserved across calls.
    SmallPtrSet<Instruction *, 128> worklist;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++) {
        worklist.insert(&*I);
        auto instruction_nonresult = nonresult.find(&*I), instruction_result = Result->find(&*I);
        assert (instruction_nonresult != nonresult.end());
        assert (instruction_result != Result->end());
        auto instruction_ain = instruction_nonresult->second.second;
        auto instruction_lin = instruction_result->second.first,
             instruction_lout = instruction_nonresult->second.first;
        computeLout(&*I, instruction_lout, Result);
        computeAin(&*I, F, instruction_ain, instruction_lin, Result, CS.isEmpty());
    }

    // Update points-to and liveness information until it converges.
    while (!worklist.empty()) {
        worklistIterations++;

        auto II = worklist.begin();
        Instruction *I = *II;
        worklist.erase(I);

        auto instruction_nonresult = nonresult.find(I), instruction_result = Result->find(I);
        assert (instruction_nonresult != nonresult.end());
        assert (instruction_result != Result->end());
        auto instruction_ain = instruction_nonresult->second.second,
             instruction_aout = instruction_result->second.second;
        auto instruction_lin = instruction_result->second.first,
             instruction_lout = instruction_nonresult->second.first;

        computeLout(I, instruction_lout, Result);
        // Lin depends on Lout, so this call needs to happen after computeLout
        // (or the current instruction should be added to the worklist when
        // computeLout returns true).
        bool addPredsToWorklist = computeLin(CS, I, instruction_ain, instruction_lin, instruction_lout);
        // Ain depends on Lin, so this call needs to happen after computeLin
        // (or the current instruction should be added to the worklist when
        // computeLin returns true).
        bool addCurrToWorklist = computeAin(I, F, instruction_ain, instruction_lin, Result, CS.isEmpty());
        // Aout depends on Lout, so this call needs to happen after computeLout
        // (or the current instruction should be added to the worklist when
        // computeLout returns true).
        bool addSuccsToWorklist = computeAout(CS, I, instruction_ain, instruction_aout, instruction_lout);

        // Add succs to worklist
        if (addSuccsToWorklist) {
            if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
                for (unsigned i = 0; i < TI->getNumSuccessors(); i++)
                    worklist.insert(TI->getSuccessor(i)->begin());
            }
            else
                worklist.insert(getNextInstruction(I));
        }

        // Add current instruction to worklist
        if (addCurrToWorklist)
            worklist.insert(I);

        // Add preds to worklist
        if (addPredsToWorklist) {
            BasicBlock *BB = I->getParent();
            Instruction *FirstInBB = BB->begin();
            if (FirstInBB == I) {
                for (pred_iterator PI = pred_begin(BB), E = pred_end(BB);
                     PI != E;
                     ++PI) {
                    BasicBlock *Pred = *PI;
                    worklist.insert(--(Pred->end()));
                }
            }
            else
                worklist.insert(getPreviousInstruction(I));
        }

        if (worklist.empty() && createdSummaryNode) {
            createdSummaryNode = false;
            // We need to rerun on stores because they might need to treat a
            // summary node differently.
            for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++)
                if (isa<StoreInst>(*I))
                    worklist.insert(&*I);
        }
    }

    // Determine the boundary information to use when running the analysis on
    // the called functions.
    for (auto I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        if (CallInst *CI = dyn_cast<CallInst>(&*I)) {
            PointsToNode *CINode = factory.getNode(CI);
            CallString newCS = CS.addCallSite(&*I);
            Function *Called = CI->getCalledFunction();

            if (Called != nullptr && !Called->isDeclaration()) {
                auto instruction_nonresult = nonresult.find(&*I);
                assert (instruction_nonresult != nonresult.end());
                auto instruction_ain = instruction_nonresult->second.second;
                auto instruction_lout = instruction_nonresult->second.first;

                // Add to the list of calls made by the function for analysis later.
                auto EntryPT = replaceActualArgumentsWithFormal(Called, CI, instruction_ain);
                auto ExitL = computeFunctionExitLiveness(CI, instruction_lout);
                bool RVL = instruction_lout->find(CINode) != instruction_lout->end();

                Calls.push_back(std::make_tuple(CI, Called, EntryPT, ExitL, RVL));
            }
        }
    }
}

bool LivenessPointsTo::runOnFunctionAt(const CallString& CS,
                                       Function *F,
                                       PointsToRelation *EntryPointsTo,
                                       LivenessSet &ExitLiveness,
                                       bool MakeReturnValuesLive,
                                       bool AlwaysRerun) {
    bool Changed = true;
    IntraproceduralPointsTo *Out = data.getPointsTo(CS, F, *EntryPointsTo, ExitLiveness, Changed);
    if (!AlwaysRerun && !Changed) {
        // If the boundary information has not changed since the analysis was
        // last run on this function, then there is no need to run it again.
        return false;
    }
    IntraproceduralPointsTo *Copy = copyPointsToMap(Out);
    SmallVector<std::tuple<CallInst *, Function *, PointsToRelation *, LivenessSet, bool>, 8> Calls;
    runOnFunction(F, CS, Out, EntryPointsTo, ExitLiveness, MakeReturnValuesLive, Calls);

    if (arePointsToMapsEqual(F, Out, Copy)) {
        // If there is a prefix with the same information, then make it
        // cyclic. If a cyclic call string is created and then the analysis is
        // rerun with a matching call string, it is removed; this deals with
        // cases where a cyclic call string is created prematurely.
        if (data.attemptMakeCyclicCallString(F, CS, Out))
            return false;

        // If there is no prefix with the same information, then we need to look
        // further down the tree until we reach a leaf or find one, so run the
        // analysis on the callees. The analysis doesn't need to be rerun on the
        // caller here for the same reason as above. If the information at a
        // callee changes, then they will rerun the analysis here.
        bool rerun = false;

        for (auto C : Calls) {
            CallInst *I;
            Function *F;
            PointsToRelation *PT;
            LivenessSet L;
            bool RVL;
            std::tie(I, F, PT, L, RVL) = C;
            rerun |= runOnFunctionAt(CS.addCallSite(I), F, PT, L, RVL, false);
        }
        if (rerun)
            return runOnFunctionAt(CS, F, EntryPointsTo, ExitLiveness, MakeReturnValuesLive, true);
        else
            return false;
    }
    else {
        // Since the information at the caller depends on the information here,
        // rerun the analysis at the caller. We don't need to rerun it on the
        // callees yet because the caller will rerun on it's callees if
        // neccessary.
        if (!CS.isEmpty())
            return true;
        // If there is no caller, then rerun the analysis here. We'll consider
        // the callees when a fixed point is reached.
        return runOnFunctionAt(CS, F, EntryPointsTo, ExitLiveness, MakeReturnValuesLive, true);
    }
}

void LivenessPointsTo::runOnModule(Module &M) {
    for (Function &F : M)
        if (!F.isDeclaration()) {
            LivenessSet L;
            runOnFunctionAt(CallString::empty(), &F, new PointsToRelation(), L, true, true);
        }
}
