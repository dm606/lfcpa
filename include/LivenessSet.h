#ifndef LFCPA_LIVENESSSET_H
#define LFCPA_LIVENESSSET_H

#include <set>

#include "PointsToNode.h"

class LivenessSet {
    public:
        typedef std::set<PointsToNode *>::iterator iterator;

        inline std::set<PointsToNode *>::const_iterator begin() const {
            return s.begin();
        }

        inline std::set<PointsToNode *>::const_iterator find(PointsToNode *N) const {
            return s.find(N);
        }

        inline std::set<PointsToNode *>::const_iterator end() const {
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

        inline std::set<PointsToNode *>::size_type erase(PointsToNode *N) {
            return s.erase(N);
        }

        inline bool insert(PointsToNode *N) {
            if (N->isUnknown)
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
