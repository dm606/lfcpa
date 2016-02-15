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

        const_pointee_iterator(const_iterator I, const_iterator E, const PointsToNode *N) : single_value(false), E(E), Value(nullptr), N(N) {
            while (I != E && I->first != N) {
                ++I;
            }
            this->I = I;
        }

        const_pointee_iterator(PointsToNode *Value) : single_value(true), Value(Value) {}

        inline reference operator*() const { return single_value ? Value : I->second; }
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
                do {
                    ++I;
                } while (I != E && I->first != N);
            }
            return *this;
        }

        inline bool atEnd() const { return single_value ? Value == nullptr : I == E; }
    private:
        // This is very ugly -- it essentially implemented two different
        // iterators with one class. However, it is significantly simpler than
        // any alternatives.
        bool single_value;
        const_iterator I, E;
        PointsToNode *Value;
        const PointsToNode *N;
    };

    class const_restriction_iterator {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef std::pair<PointsToNode *, PointsToNode *> value_type;
        typedef signed difference_type;
        typedef std::pair<PointsToNode *, PointsToNode *> const* pointer;
        typedef std::pair<PointsToNode *, PointsToNode *> const& reference;

        const_restriction_iterator(const_iterator I, const_iterator E, std::set<PointsToNode *>::const_iterator DI, std::set<PointsToNode *>::const_iterator DE) : I(I), E(E), DI(DI), DE(DE), useSinglePointee(false) {
            advance_iterators();
        }

        inline reference operator*() const {
            if (useSinglePointee)
                return singlePointeePair;
            else
                return *I;
        }
        inline pointer operator->() const { return &operator*(); }

        inline bool operator==(const const_restriction_iterator &Y) const {
            if (useSinglePointee)
                return Y.useSinglePointee && singlePointeePair == Y.singlePointeePair;
            else
                return !Y.useSinglePointee && (I == Y.I || (I == E && Y.I == Y.E));
        }
        inline bool operator !=(const const_restriction_iterator &Y) const {
            return !operator==(Y);
        }

        const_restriction_iterator &operator++() {
            if (useSinglePointee) {
                useSinglePointee = false;
                ++DI;
            }
            else
                ++I;
            advance_iterators();
            return *this;
        }

        inline bool atEnd() const { return I == E; }
    private:
        inline void advance_iterators() {
            assert(!useSinglePointee);
            // Increment I zero or more times, until its first component is in
            // DI..DE (or until the end is reached), and advance DI until it
            // reaches I (or until the end is reached).
            std::less<PointsToNode *> l;
            while (I != E && DI != DE && I->first != *DI) {
                // Advance DI until it is greater than or equal to I->first, or
                // a value that has a single pointee is found.
                while (DI != DE && !(*DI)->singlePointee() && l(*DI, I->first)) ++DI;
                if (DI == DE)
                    break;
                else if ((*DI)->singlePointee()) {
                    useSinglePointee = true;
                    singlePointeePair = std::make_pair(*DI, (*DI)->getSinglePointee());
                    break;
                }
                // Advance I until I->first is greater than or equal to DI.
                while (I != E && l(I->first, *DI)) ++I;
            }

            if (DI == DE)
                I = E;
        }

        const_iterator I, E;
        std::set<PointsToNode *>::const_iterator DI, DE;
        bool useSinglePointee;
        std::pair<PointsToNode *, PointsToNode *> singlePointeePair;
    };

    inline void insertAll(PointsToRelation &R) {
        s.insert(R.s.begin(), R.s.end());
    }

    inline void clear() {
        s.clear();
    }

    inline bool insert(const std::pair<PointsToNode *, PointsToNode *> &N) {
        if (N.first->singlePointee()) {
            assert(N.first->getSinglePointee() == N.second || isa<UnknownPointsToNode>(N.second) && "The pair given is incorrect.");
            return false;
        }

        if (isa<UnknownPointsToNode>(N.first) || !N.first->hasPointerType())
            return false;

        return s.insert(N).second;
    }

    inline bool operator==(const PointsToRelation &R) const {
        return s==R.s;
    }

    inline bool operator!=(const PointsToRelation &R) const {
        return s!=R.s;
    }

    inline const_pointee_iterator pointee_begin(const PointsToNode *N) {
        if (N->singlePointee())
            return const_pointee_iterator(N->getSinglePointee());
        else
            return const_pointee_iterator(s.begin(), s.end(), N);
    }

    inline const_pointee_iterator pointee_end(const PointsToNode *N) {
        if (N->singlePointee())
            return const_pointee_iterator(nullptr);
        else
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

    void dump() const;
private:
    std::set<std::pair<PointsToNode *, PointsToNode *>> s;
};

#endif
