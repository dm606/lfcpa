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
            return s.erase(N);
        }

        inline bool insert(PointsToNode *N) {
            if (isa<UnknownPointsToNode>(N))
                return false;

            return s.insert(N).second;
        }

        inline void insert (iterator begin, iterator end) {
            s.insert(begin, end);
        }

        inline bool operator==(const LivenessSet &R) const {
            return s==R.s;
        }

        inline bool operator!=(const LivenessSet &R) const {
            return s!=R.s;
        }
    private:
        std::set<PointsToNode *> s;
};

#endif
