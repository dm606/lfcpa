#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"

#include "PointsToData.h"

ProcedurePointsTo *PointsToData::getAtFunction(const Function *F) const {
    auto result = data.find(F);
    assert (result != data.end() && "The points-to data does not contain an entry for the specified function.");
    return result->second;
}

bool arePointsToMapsEqual(const Function *F, IntraproceduralPointsTo *a, IntraproceduralPointsTo &b) {
    for (const_inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        auto p1 = a->find(&*I), p2 = b.find(&*I);
        assert(p1 != a->end() && p2 != b.end() && "Invalid points-to relations");
        LivenessSet *l1 = p1->second.first, *l2 = p2->second.first;
        if (*l1 != *l2)
            return false;
        PointsToRelation *r1 = p1->second.second, *r2 = p2->second.second;
        if (*r1 != *r2)
            return false;
    }
    return true;
}

IntraproceduralPointsTo copyPointsToMap(IntraproceduralPointsTo *M) {
    IntraproceduralPointsTo Result;
    for (auto P : *M) {
        LivenessSet *L = new LivenessSet(*P.second.first);
        PointsToRelation *R = new PointsToRelation(*P.second.second);
        Result.insert(std::make_pair(P.first, std::make_pair(L, R)));
    }
    return Result;
}

IntraproceduralPointsTo *PointsToData::getPointsTo(const CallString &CS, const Function *F, PointsToRelation &EntryPT, LivenessSet &ExitL, bool &Changed) {
    assert (!CS.isCyclic() && "Information has already been computed.");

    auto P = data.find(F);
    ProcedurePointsTo *Pointsto;
    if (P == data.end()) {
        Pointsto = new ProcedurePointsTo();
        data.insert(std::make_pair(F, Pointsto));
    }
    else
        Pointsto = P->second;

    for (auto I = Pointsto->begin(), E = Pointsto->end(); I != E; ++I) {
        CallString ICS = std::get<0>(*I);
        if (ICS.isCyclic() && ICS.matches(CS)) {
            // We need to remove the call string completely here because it may
            // have been made cyclic prematurely. It is possible to break here
            // because the removal of call strings in
            // attemptMakeCyclicCallString ensures that others do not match.
            Pointsto->erase(I);
            break;
        }
        if (CS == ICS) {
            auto IData = std::get<1>(*I);
            PointsToRelation IPT = std::get<2>(*I);
            LivenessSet IL = std::get<3>(*I);
            if (IPT == EntryPT && IL == ExitL) {
                Changed = false;
                return IData;
            }
            else {
                *I = std::make_tuple(ICS, IData, IPT, IL);
                Changed = true;
                return IData;
            }
        }
    }

    // The call string wasn't found.
    IntraproceduralPointsTo *Out = new IntraproceduralPointsTo();
    for (const_inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I)
        Out->insert({&*I, {new LivenessSet(), new PointsToRelation()}});
    Pointsto->push_back(std::make_tuple(CS, Out, EntryPT, ExitL));
    Changed = true;
    return Out;
}

bool PointsToData::attemptMakeCyclicCallString(const Function *F, const CallString &CS, IntraproceduralPointsTo *Out) {
    assert(!CS.isCyclic() && "The call string must be non-cyclic");

    if (CS.isEmpty())
        return false;

    auto LastCall = CS.getLastCall();
    auto LastCalledFunction = CS.getLastCalledFunction();

    auto pair = data.find(F);
    assert(pair != data.end());

    ProcedurePointsTo *V = pair->second;
    // If the set already contains a pair of a call string s such that
    // CS = S . S' and a points to map which matches Out, then the call
    // string in the pair can be replaced with S . S'*, since adding an
    // extra S' to the end does not change the points to map.
    auto I = V->begin(), E = V->end();
    for (; I != E; ++I) {
        auto ICS = std::get<0>(*I);
        auto IData = std::get<1>(*I);
        auto IPT = std::get<2>(*I);
        auto IL = std::get<3>(*I);
        if ((ICS.isEmpty() || ICS.getLastCall() == LastCall || (LastCalledFunction != nullptr && ICS.getLastCalledFunction() == LastCalledFunction)) &&
            CS.isNonCyclicPrefix(ICS) &&
            arePointsToMapsEqual(F, IData, *Out)) {
            CallString newCS = CS.createCyclicFromPrefix(ICS);
            *I = std::make_tuple(newCS, Out, IPT, IL);
            break;
        }
    }

    if (I != E) {
        auto ICS = std::get<0>(*I);
        CallString R = ICS;
        // Remove all call strings that match the inserted one.
        for (auto I2 = V->begin(); I2 != E; ) {
            if (I != I2 && !std::get<0>(*I2).isCyclic() && R.matches(std::get<0>(*I2))) {
                I2 = V->erase(I2);
                E = V->end();
            }
            else
                ++I2;
        }
        return true;
    }
    else
        return false;
}

bool PointsToData::hasDataForFunction(const Function *F) const {
    auto I = data.find(F);
    if (I == data.end())
        return false;
    return !I->second->empty();
}

IntraproceduralPointsTo *PointsToData::get(const Function *F, const CallString &CS) const {
    assert(!CS.isCyclic() && "We can only check against non-cyclic call strings.");

    auto I = data.find(F);
    if (I == data.end())
        return nullptr;

    ProcedurePointsTo *V = I->second;
    for (auto P : *V) {
        auto PCS = std::get<0>(P);
        auto PData = std::get<1>(P);
        if (PCS.matches(CS))
            return PData;
    }

    return nullptr;
}
