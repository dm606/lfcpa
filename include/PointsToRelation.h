#ifndef LFCPA_POINTSTORELATION_H
#define LFCPA_POINTSTORELATION_H

#include <set>

#include "LivenessSet.h"
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
        typedef signed difference_type;
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

    class const_restriction_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::pair<PointsToNode *, PointsToNode *> value_type;
        typedef signed difference_type;
        typedef std::pair<PointsToNode *, PointsToNode *> const* pointer;
        typedef std::pair<PointsToNode *, PointsToNode *> const& reference;

        const_restriction_iterator(const_iterator I, const_iterator E, std::set<PointsToNode *>::const_iterator DI, std::set<PointsToNode *>::const_iterator DE) : I(I), E(E), DI(DI), DE(DE) {
            advance_iterators();
        }

        inline reference operator*() const { return *I; }
        inline pointer operator->() const { return &operator*(); }

        inline bool operator==(const const_restriction_iterator &Y) const {
            return I == Y.I || (I == E && Y.I == Y.E);
        }
        inline bool operator !=(const const_restriction_iterator &Y) const {
            return !operator==(Y);
        }

        const_restriction_iterator &operator++() {
            ++I;
            advance_iterators();
            return *this;
        }

        inline bool atEnd() const { return I == E; }
    private:
        inline void advance_iterators() {
            // Increment I zero or more times, until its first component is in
            // DI..DE (or until the end is reached), and advance DI until it
            // reaches I (or until the end is reached).
            std::less<PointsToNode *> l;
            while (I != E && DI != DE && I->first != *DI) {
                // Advance DI until it is greater than or equal to I->first.
                while (DI != DE && l(*DI, I->first)) ++DI;
                if (DI == DE)
                    break;
                // Advance I until I->first is greater than or equal to DI.
                while (I != E && l(I->first, *DI)) ++I;
            }

            if (DI == DE)
                I = E;
        }

        const_iterator I, E;
        std::set<PointsToNode *>::const_iterator DI, DE;
    };

    class const_global_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::pair<PointsToNode *, PointsToNode *> value_type;
        typedef signed difference_type;
        typedef std::pair<PointsToNode *, PointsToNode *> const* pointer;
        typedef std::pair<PointsToNode *, PointsToNode *> const& reference;

        const_global_iterator(const_iterator I, const_iterator E) : E(E) {
            while (I != E && !I->first->isGlobalAddress()) {
                ++I;
            }
            this->I = I;
        }

        inline reference operator*() const { return *I; }
        inline pointer operator->() const { return &operator*(); }

        inline bool operator==(const const_global_iterator &Y) const {
            return I == Y.I || (I == E && Y.I == Y.E);
        }
        inline bool operator !=(const const_global_iterator &Y) const {
            return !operator==(Y);
        }

        const_global_iterator &operator++() {
            do {
                ++I;
            } while (I != E && !I->first->isGlobalAddress());
            return *this;
        }

        inline bool atEnd() const { return I == E; }
    private:
        const_iterator I, E;
    };
/*
    inline const_iterator begin() const {
        return s.begin();
    }

    inline const_iterator end() const {
       return s.end();
    }
*/
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

    inline const_restriction_iterator restriction_begin(const std::set<PointsToNode *>& S) {
        return const_restriction_iterator(s.begin(), s.end(), S.begin(), S.end());
    }

    inline const_restriction_iterator restriction_end(const std::set<PointsToNode *>& S) {
        return const_restriction_iterator(s.end(), s.end(), S.begin(), S.end());
    }

    inline const_restriction_iterator restriction_begin(const LivenessSet &S) {
        return const_restriction_iterator(s.begin(), s.end(), S.begin(), S.end());
    }

    inline const_restriction_iterator restriction_end(const LivenessSet &S) {
        return const_restriction_iterator(s.end(), s.end(), S.begin(), S.end());
    }

    inline const_restriction_iterator restriction_begin(const LivenessSet *S) {
        return const_restriction_iterator(s.begin(), s.end(), S->begin(), S->end());
    }

    inline const_restriction_iterator restriction_end(const LivenessSet *S) {
        return const_restriction_iterator(s.end(), s.end(), S->begin(), S->end());
    }

    inline const_global_iterator global_begin() {
        return const_global_iterator(s.begin(), s.end());
    }

    inline const_global_iterator global_end() {
        return const_global_iterator(s.end(), s.end());
    }

    void dump() const;
private:
    std::set<std::pair<PointsToNode *, PointsToNode *>> s;
};

#endif
