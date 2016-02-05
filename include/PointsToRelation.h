#ifndef LFCPA_POINTSTORELATION_H
#define LFCPA_POINTSTORELATION_H

#include <set>

#include "PointsToNode.h"

class PointsToRelation {
    public:
        typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::iterator iterator;
        typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::const_iterator const_iterator;
        typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::size_type size_type;

        inline const_iterator begin() const {
            return s.begin();
        }

        inline const_iterator end() const {
            return s.end();
        }

        inline void clear() {
            s.clear();
        }

        inline bool insert(const std::pair<PointsToNode *, PointsToNode *> &N) {
            if (isa<UnknownPointsToNode>(N.first) || !N.first->hasPointerType())
                return false;

            return s.insert(N).second;
        }

        inline void insert(iterator begin, iterator end) {
            s.insert(begin, end);
        }

        inline bool operator==(const PointsToRelation &R) const {
            return s==R.s;
        }

        inline bool operator!=(const PointsToRelation &R) const {
            return s!=R.s;
        }
    private:
        std::set<std::pair<PointsToNode *, PointsToNode *>> s;
};

#endif
