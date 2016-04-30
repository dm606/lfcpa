#ifndef LFCPA_LIVENESSSET_H
#define LFCPA_LIVENESSSET_H

#include <set>

#include "PointsToNode.h"

class LivenessSet;

class PointsToNodeSet {
    public:
    class const_iterator {
        public:
            typedef std::forward_iterator_tag iterator_category;
            typedef PointsToNode* value_type;
            typedef signed difference_type;
            typedef PointsToNode* const* pointer;
            typedef PointsToNode* const& reference;

            const_iterator(const bdd B, bool AtEnd) : CurrentVar(-1), RelevantNodes(B) {
                if (!AtEnd)
                    findNext();
            }

            inline reference operator*() const { return PointsToNode::AllNodes[CurrentVar]; }
            inline pointer operator->() const { return &operator*(); }

            inline bool operator==(const const_iterator &Y) const {
                return CurrentVar == Y.CurrentVar || (atEnd() && Y.atEnd());
            }

            inline bool operator!=(const const_iterator &Y) const {
                return !operator==(Y);
            }

            const_iterator &operator++() {
                findNext();
                return *this;
            }

            inline bool atEnd() const { return CurrentVar < 0; }
        private:
            inline void findNext() {
                // RelevantNodes always represents the set of remaining nodes.
                // We get the next one and then remove it from the set.

                CurrentVar = fdd_scanvar(RelevantNodes, LeftDomain);
                assert((CurrentVar < 0) == (RelevantNodes == bdd_false()));

                if (CurrentVar >= 0)
                    RelevantNodes &= !fdd_ithvar(LeftDomain, CurrentVar);
            }
            int CurrentVar;
            bdd RelevantNodes;
        };

        friend class PointsToRelation;

        PointsToNodeSet() : B(bdd_false()) {}

        inline const_iterator begin() const {
            return const_iterator(B, false);
        }

        inline bool contains(const PointsToNode *N) const {
            bdd R = B & N->Left;
            assert(R == N->Left || R == bdd_false());
            return R != bdd_false();
        }

        inline const_iterator end() const {
            return const_iterator(B, true);
        }

        inline void clear() {
            B = bdd_false();
        }

        virtual void insert(const PointsToNode *N) {
            B |= N->Left;
        }

        inline bool operator==(const PointsToNodeSet &R) const {
            return B == R.B;
        }

        inline bool operator!=(const PointsToNodeSet &R) const {
            return B != R.B;
        }

        void dump() const;

        void eraseNonSummaryNodes(const CallString &CS) {
            B &= PointsToNode::getSummaryNodeBDD(CS);
        }

        void insertSummaryNodesFrom(const CallString &CS, const PointsToNodeSet &L) {
            // Inserts all of the summary nodes in L into this. Implemented as
            // a single operation for efficiency.
            B |= L.B & PointsToNode::getSummaryNodeBDD(CS);
        }

        inline void insertSubtraction(const LivenessSet &, const PointsToNodeSet &);
        inline void insertIntersection(const LivenessSet &, const PointsToNodeSet &);
        inline void insertAll(const LivenessSet &);

        virtual ~PointsToNodeSet() {}
    protected:
        bdd B;
};

class LivenessSet : public PointsToNodeSet {
    public:
        inline void erase(PointsToNode *N) {
            // When we kill a node, it's children (i.e. GEPs) are also killed.
            for (PointsToNode *Child : N->children) {
                assert(isa<GEPPointsToNode>(Child) && "All children of PointsToNodes should be GEPs");
                B &= !Child->Left;
            }

            B &= !N->Left;
        }

        void insert(const PointsToNode *N) override {
            if (N->singlePointee() || (!N->hasPointerType() && !N->isAlwaysSummaryNode()) || isa<UnknownPointsToNode>(N))
                return;
            PointsToNodeSet::insert(N);
        }
};

inline void PointsToNodeSet::insertAll(const LivenessSet &L) {
    B |= L.B;
}

inline void PointsToNodeSet::insertIntersection(const LivenessSet &L1, const PointsToNodeSet &L2) {
    B |= L1.B & L2.B;
}

inline void PointsToNodeSet::insertSubtraction(const LivenessSet &L1, const PointsToNodeSet &L2) {
    B |= L1.B - L2.B;
}

#endif
