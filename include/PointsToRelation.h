#ifndef LFCPA_POINTSTORELATION_H
#define LFCPA_POINTSTORELATION_H

#include <set>
#include <bdd.h>
#include <fdd.h>

#include "LivenessSet.h"
#include "PointsToNode.h"

class PointsToRelation {
public:
    typedef SmallVector<PointsToNode *, 1024>::const_iterator const_node_iterator;
    class const_pointee_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef PointsToNode* value_type;
        typedef signed difference_type;
        typedef PointsToNode* const* pointer;
        typedef PointsToNode* const& reference;

        const_pointee_iterator(const_node_iterator I, const_node_iterator E, PointsToNode *N, const bdd B) :
                single_value(false),
                I(I),
                E(E),
                Value(nullptr),
                RelevantNodes(B & N->Left) {
            if (I != E)
                advanceToNext();
        }

        const_pointee_iterator(PointsToNode *Value) : single_value(true), Value(Value) {}

        inline reference operator*() const { return single_value ? Value : *I; }
        inline pointer operator->() const { return &operator*(); }

        inline bool operator==(const const_pointee_iterator &Y) const {
            assert (single_value == Y.single_value);
            if (single_value)
                return Value == Y.Value;
            else
                return I == Y.I || (I == E && Y.I == Y.E);
        }
        inline bool operator !=(const const_pointee_iterator &Y) const {
            return !operator==(Y);
        }

        const_pointee_iterator &operator++() {
            if (single_value) {
                assert(Value);
                Value = nullptr;
            }
            else {
                ++I;
                advanceToNext();
            }
            return *this;
        }

        inline bool atEnd() const { return single_value ? Value == nullptr : I == E; }
    private:
        inline void advanceToNext() {
            while (I != E && (RelevantNodes & (*I)->Right) == bdd_false())
                ++I;
        }
        // This is very ugly -- it essentially implements two different
        // iterators with one class. However, it is significantly simpler than
        // any alternatives.
        bool single_value;
        const_node_iterator I;
        const_node_iterator E;
        PointsToNode *Value;
        bdd RelevantNodes;
    };

    inline std::set<PointsToNode *> getPointees(PointsToNode *N) const {
        bdd B = s & N->Left;
        std::set<PointsToNode *> Result;
        if (N->singlePointee())
            Result.insert(N->getSinglePointee());
        else {
            for (PointsToNode *Pointee : PointsToNode::AllNodes) {
                bdd NodeInRelation = B & Pointee->Right;
                if (NodeInRelation != bdd_false())
                    Result.insert(Pointee);
            }
        }
        return Result;
    }

    inline const_pointee_iterator pointee_begin(PointsToNode *N) const {
        if (N->singlePointee())
            return const_pointee_iterator(N->getSinglePointee());
        else
            return const_pointee_iterator(PointsToNode::AllNodes.begin(), PointsToNode::AllNodes.end(), N, s);
    }

    inline const_pointee_iterator pointee_end(PointsToNode *N) const {
        if (N->singlePointee())
            return const_pointee_iterator(nullptr);
        else
            return const_pointee_iterator(PointsToNode::AllNodes.end(), PointsToNode::AllNodes.end(), N, s);
    }

    inline bool hasPointee(PointsToNode *N) {
        if (N->singlePointee())
            return true;
        else
            return (s & N->Left) != bdd_false();
    }

    inline bool operator==(const PointsToRelation &R) const {
        return s == R.s;
    }

    inline bool operator!=(const PointsToRelation &R) const {
        return s != R.s;
    }

    inline void insertLoadPairs(const PointsToRelation &Ain, const PointsToNode *Load, const PointsToNode *Pointer) {
        // If Pointer points to N and N points to M, then we make Load point to
        // M. This is implemented as a single operation in PointsToRelation for
        // efficiency.

        // Ain.s is a subset of Left * Right but we need a version that is a
        // subset of Intermediate * Right.
        bdd AinIR = bdd_replace(Ain.s, PointsToNode::LeftToIntermediate);

        // We assume here that pointees of Pointer can't have singlePointee set
        // to true.
        if (Pointer->singlePointee()) {
            // The pair (Load, Pointer->getSinglePointee())
            bdd LoadPointee = Load->Left & Pointer->getSinglePointee()->Intermediate;

            // Do the dereferencing.
            bdd NewPairs = bdd_relprod(LoadPointee, AinIR, fdd_ithset(IntermediateDomain));

            s |= NewPairs;
        }
        else {
            // The pair (Load, Pointer).
            bdd LoadPointer = Load->Left & Pointer->Intermediate;

            // Turn pairs (Pointer, N) into (Load, N).
            bdd BeforeDeref = bdd_replace(bdd_relprod(LoadPointer, AinIR, fdd_ithset(IntermediateDomain)), PointsToNode::RightToIntermediate);

            // Do the dereferencing.
            bdd NewPairs = bdd_relprod(BeforeDeref, AinIR, fdd_ithset(IntermediateDomain));

            s |= NewPairs;
        }
    }

    inline void insertStorePairs(const PointsToRelation &Ain, const PointsToNode *Pointer, const PointsToNode *Value, const LivenessSet &L) {
        // If Ain contains pairs (Pointer, N) and (Value, M), with M live, then
        // insert the pair (N, M). This is implemented as a single operation for
        // efficiency.

        // Get a subset of Intermediate * Left that contains the pairs (Pointer, N).
        bdd PointerPairs =
            Pointer->singlePointee()
            ? Pointer->Intermediate & Pointer->getSinglePointee()->Left
            : bdd_replace(bdd_replace(Ain.s, PointsToNode::LeftToIntermediate), PointsToNode::RightToLeft);

        // Get a subset of Left * Right that contains the pairs (Value, M).
        bdd ValuePairs =
            Value->singlePointee()
            ? Value->Left & Value->getSinglePointee()->Right
            : Ain.s;

        // Now get a subset of Intermediate * Right that contains (Pointer, M)
        // for each (Value, M) in ValuePairs.
        bdd B = bdd_relprod(Pointer->Intermediate & Value->Left, ValuePairs, fdd_ithset(LeftDomain));

        // Get the pairs (N, M) and union in the important ones.
        unionRestrictedBDD(bdd_relprod(PointerPairs, B, fdd_ithset(IntermediateDomain)), L.begin(), L.end());
    }

    inline void insertAssignmentPairs(const PointsToRelation &Ain, const PointsToNode *LHS, const PointsToNode *RHS) {
        // If Ain contains the pair (RHS, N), then insert the pair (LHS, N).
        // This is implemented as a single operation for efficiency.

        // Get a subset of Intermediate * Right containing the pairs (RHS, N).
        bdd RHSPairs =
            RHS->singlePointee()
            ? RHS->Intermediate & RHS->getSinglePointee()->Right
            : bdd_replace(Ain.s, PointsToNode::LeftToIntermediate);

        // Get the pairs (LHS, N) and union.
        s |= bdd_relprod(LHS->Left & RHS->Intermediate, RHSPairs, fdd_ithset(IntermediateDomain));
    }

    inline void insert(const PointsToNode *Pointer, const PointsToNode *Pointee) {
        if (Pointer->singlePointee()) {
            assert((Pointer->getSinglePointee() == Pointee || isa<UnknownPointsToNode>(Pointee)) && "Pointer shouldn't point to Pointee.");
            return;
        }

        // insertLoadPairs requires this to be true.
        assert(!Pointee->singlePointee());

        // If a value is a summary node, it may be, for example, an array of
        // pointers, and therefore may have pointees.
        if (isa<UnknownPointsToNode>(Pointer) || (!Pointer->hasPointerType() && !Pointer->isAlwaysSummaryNode()))
            return;

        s |= (Pointer->Left & Pointee->Right);
    }

    inline void replaceWith(const PointsToRelation &R) {
        s = R.s;
    }

    inline void insertAll(const PointsToRelation &R) {
        s |= R.s;
    }

    inline void unionRelationRestriction(const PointsToRelation &R, const std::set<PointsToNode *> &L) {
        // Restrict the domain of R to the nodes in L, and then union the
        // result into this.
        unionRestrictedBDD(R.s, L.begin(), L.end());
    }

    inline void unionRelationRestriction(const PointsToRelation &R, const LivenessSet &L) {
        // Restrict the domain of R to the nodes in L, and then union the
        // result into this.
        unionRestrictedBDD(R.s, L.begin(), L.end());
    }

    inline void unionComplementRestriction(const PointsToRelation &R, const std::set<PointsToNode *> &L) {
        // Restrict the domain of R to the nodes not in L, and then union the
        // result into this.
        // The idea is to construct the BDD for the relation
        //   {(x, x) | x in L}
        // and then to compose R with this, and then subtract this from R.
        bdd RestrictedId = bdd_false();
        for (PointsToNode *N : L)
            RestrictedId |= N->Left & N->Intermediate;
        bdd RIR = bdd_replace(R.s, PointsToNode::LeftToIntermediate);

        s |= R.s - bdd_relprod(RestrictedId, RIR, fdd_ithset(IntermediateDomain));
    }

    void dump() const;
private:
    bdd s = bdd_false();

    inline void unionRestrictedBDD(bdd B, std::set<PointsToNode *>::const_iterator Begin, std::set<PointsToNode *>::const_iterator End) {
        // Restrict the domain of R to the nodes in the range [Begin, End), and
        // then union the result into this.
        // The idea is to construct the BDD for the relation
        //   {(x, x) | x in L}
        // and then to compose R with this.
        bdd RestrictedId = bdd_false();
        for (auto I = Begin; I != End; ++I)
            RestrictedId |= (*I)->Left & (*I)->Intermediate;
        bdd RIR = bdd_replace(B, PointsToNode::LeftToIntermediate);

        s |= bdd_relprod(RestrictedId, RIR, fdd_ithset(IntermediateDomain));  
    }
};

#endif
