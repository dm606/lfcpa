#ifndef LFCPA_POINTSTORELATION_H
#define LFCPA_POINTSTORELATION_H

#include <set>

#include "PointsToNode.h"

class PointsToRelation {
public:
    typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::iterator iterator;
    typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::const_iterator const_iterator;
    typedef std::set<std::pair<PointsToNode *, PointsToNode *>>::size_type size_type;

    class const_pointee_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef PointsToNode* value_type;
        typedef PointsToNode* const* pointer;
        typedef PointsToNode* const& reference;

        const_pointee_iterator (const_iterator I, const_iterator E, const PointsToNode *N) : E(E), N(N) {
            while (I != E && I->first != N) {
                ++I;
            }
            this->I = I;
        }

        inline reference operator*() const { return I->second; }
        inline pointer operator->() const { return &operator*(); }

        inline bool operator==(const const_pointee_iterator &Y) const {
            return I == Y.I || (I == E && Y.I == Y.E);
        }
        inline bool operator !=(const const_pointee_iterator &Y) const {
            return !operator==(Y);
        }

        const_pointee_iterator &operator++() {
            do {
                ++I;
            } while (I != E && I->first != N);
            return *this;
        }

        inline bool atEnd() const { return I == E; }
    private:
        const_iterator I, E;
        const PointsToNode *N;
    };

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

    inline const_pointee_iterator pointee_begin(const PointsToNode *N) {
        return const_pointee_iterator(s.begin(), s.end(), N);
    }

    inline const_pointee_iterator pointee_end(const PointsToNode *N) {
        return const_pointee_iterator(s.end(), s.end(), N);
    }

    void dump() const;
private:
    std::set<std::pair<PointsToNode *, PointsToNode *>> s;
};

#endif
