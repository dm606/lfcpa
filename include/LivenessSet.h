#ifndef LFCPA_LIVENESSSET_H
#define LFCPA_LIVENESSSET_H

#include <set>

#include "PointsToNode.h"

class LivenessSet {
    public:
        typedef std::set<PointsToNode *>::iterator iterator;
        typedef std::set<PointsToNode *>::const_iterator const_iterator;
        typedef std::set<PointsToNode *>::size_type size_type;

        inline const_iterator begin() const {
            return s.begin();
        }

        inline const_iterator find(PointsToNode *N) const {
            return s.find(N);
        }

        inline const_iterator end() const {
            return s.end();
        }

        inline bool empty() const {
            return s.empty();
        }

        inline int size() const {
            return s.size();
        }

        inline void clear() {
            s.clear();
        }

        inline size_type erase(PointsToNode *N) {
            // When we kill a node, it's children (i.e. GEPs) are also killed.
            for (PointsToNode *Child : N->children) {
                assert(isa<GEPPointsToNode>(Child) && "All children of PointsToNodes should be GEPs");
                s.erase(Child);
            }

            return s.erase(N);
        }

        inline bool insert(PointsToNode *N) {
            if (N->singlePointee() || (!N->hasPointerType() && !N->isSummaryNode()) || isa<UnknownPointsToNode>(N))
                return false;

            return s.insert(N).second;
        }

        inline void insertAll(LivenessSet &L) {
            s.insert(L.begin(), L.end());
        }

        inline bool operator==(const LivenessSet &R) const {
            return s==R.s;
        }

        inline bool operator!=(const LivenessSet &R) const {
            return s!=R.s;
        }

        void dump() const;
    private:
        std::set<PointsToNode *> s;
};

#endif
