#include <set>

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#include "../include/LivenessPointsToMisc.h"
#include "../include/IntraproceduralLivenessPointsTo.h"
#include "../include/PointsToNode.h"

void IntraproceduralLivenessPointsTo::subtractKill(std::set<PointsToNode *> &Lin,
                                    Instruction *I,
                                    PointsToRelation *Ain) {
    Lin.erase(factory.getNode(I));
    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *Ptr = SI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);

        bool moreThanOne = false;
        PointsToNode *PointedTo = nullptr;

        for (auto P : *Ain) {
            if (P.first == PtrNode) {
                if (PointedTo == nullptr)
                    PointedTo = P.second;
                else {
                    moreThanOne = true;
                    break;
                }
            }
        }

        if (!moreThanOne) {
            if (PointedTo == nullptr || PointedTo == factory.getUnknown()) {
                // We have no information about what Ptr can point to, so kill
                // everything.
                Lin.clear();
            }
            else {
                // Ptr must point to PointedTo, so we can do a strong update
                // here.
                Lin.erase(PointedTo);
            }
        }

        // If there is more than one value that is possibly pointed to by Ptr,
        // then we need to perform a weak update, so we don't kill anything
        // else.
    }
}

void IntraproceduralLivenessPointsTo::unionRef(std::set<PointsToNode *>& Lin,
                                Instruction *I,
                                std::set<PointsToNode *>* Lout,
                                PointsToRelation* Ain) {
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        // We only consider the pointer and the possible values in memory to be
        // ref'd if the load is live.
        if (Lout->find(factory.getNode(I)) != Lout->end()) {
            Value *Ptr = LI->getPointerOperand();
            PointsToNode *PtrNode = factory.getNode(Ptr);
            Lin.insert(PtrNode);
            for (auto &P : *Ain)
                if (P.first == PtrNode)
                    Lin.insert(P.second);
        }
    }
    else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *Ptr = SI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);
        Lin.insert(PtrNode);

        // We only consider the stored value to be ref'd if at least one of the
        // values that can be pointed to by x is live.
        for (auto P : *Ain) {
            if (P.first == PtrNode && Lout->find(P.second) != Lout->end()) {
                Lin.insert(factory.getNode(SI->getValueOperand()));
                break;
            }
        }
    }
    else if (isa<PHINode>(I) || isa<SelectInst>(I)) {
        // We only consider the operands of a PHI node or select instruction to
        // be ref'd if I is live.
        if (Lout->find(factory.getNode(I)) != Lout->end()) {
            for (Use &U : I->operands())
                if (Value *Operand = dyn_cast<Value>(U))
                    Lin.insert(factory.getNode(Operand));
        }
    }
    else {
        // If the instruction is not a load or a store, we consider all of it's
        // operands to be ref'd, even if the instruction is not live.
        for (Use &U : I->operands())
            if (Value *Operand = dyn_cast<Value>(U))
                Lin.insert(factory.getNode(Operand));
    }
}

void IntraproceduralLivenessPointsTo::unionRelationRestriction(PointsToRelation &Result,
                                                PointsToRelation *Aout,
                                                std::set<PointsToNode *> *Lin) {
    for (auto &P : *Aout)
        if (Lin->find(P.first) != Lin->end())
            Result.insert(P);
}

std::set<PointsToNode *>
IntraproceduralLivenessPointsTo::getRestrictedDef(Instruction *I,
                                   PointsToRelation *Ain,
                                   std::set<PointsToNode *> *Lout) {
    std::set<PointsToNode *> s;
    if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *Ptr = SI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);

        for (auto &P : *Ain)
            if (P.first == PtrNode && Lout->find(P.second) != Lout->end())
                s.insert(P.second);
    }
    else {
        switch (I->getOpcode()) {
            case Instruction::Alloca:
            case Instruction::Load:
            case Instruction::PHI:
            case Instruction::Select:
                {
                    PointsToNode *N = factory.getNode(I);
                    if (Lout->find(N) != Lout->end())
                        s.insert(N);
                }
                break;
            default:
                break;
        }
    }
    return s;
}

void IntraproceduralLivenessPointsTo::insertPointedToBy(std::set<PointsToNode *> &S,
                                         Value *V,
                                         PointsToRelation *Ain) {
    PointsToNode *VNode = factory.getNode(V);
    for (auto &P : *Ain)
        if (P.first == VNode)
            S.insert(P.second);
}

std::set<PointsToNode *> IntraproceduralLivenessPointsTo::getPointee(Instruction *I,
                                                      PointsToRelation *Ain) {
    std::set<PointsToNode *> s;
    if (LoadInst *LI = dyn_cast<LoadInst>(I)) {
        Value *Ptr = LI->getPointerOperand();
        PointsToNode *PtrNode = factory.getNode(Ptr);
        std::set<PointsToNode *> t;
        for (auto &P : *Ain)
            if (P.first == PtrNode)
                t.insert(P.second);
        for (auto &P : *Ain)
            if (t.find(P.first) != t.end())
                s.insert(P.second);
    }
    else if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        Value *V = SI->getValueOperand();
        insertPointedToBy(s, V, Ain);
    }
    else if (AllocaInst *AI = dyn_cast<AllocaInst>(I)) {
        // If the instruction is an alloca, then we consider the pointer to be
        // to a special location which does not correspond to any Value and is
        // disjoint from all other locations.
        s.insert(factory.getAllocaNode(AI));
    }
    else if (PHINode *Phi = dyn_cast<PHINode>(I)) {
        // The result of the phi can point to anything that an operand of the
        // phi can point to.
        for (auto &V : Phi->incoming_values())
            insertPointedToBy(s, V, Ain);
    }
    else if (SelectInst *SI = dyn_cast<SelectInst>(I)) {
        // The result of the select can point to anything that one of the
        // selected values can point to.
        insertPointedToBy(s, SI->getTrueValue(), Ain);
        insertPointedToBy(s, SI->getFalseValue(), Ain);
    }
    return s;
}

void IntraproceduralLivenessPointsTo::unionCrossProduct(PointsToRelation &Result,
                                         std::set<PointsToNode *> &A,
                                         std::set<PointsToNode *> &B) {
    for (auto &X : A)
        for (auto &Y : B)
            Result.insert(std::make_pair(X, Y));
}

void IntraproceduralLivenessPointsTo::runOnFunction(Function &F) {
    // Points-to information
    DenseMap<const Instruction *, PointsToRelation *> ain, aout;
    // Liveness information
    DenseMap<const Instruction *, std::set<PointsToNode*> *> lin, lout;

    // Create vectors to store the points-to information in.
    for (auto &BB : F) {
        for (auto &I : BB) {
            ain.insert(std::make_pair(&I, new PointsToRelation()));
            aout.insert(std::make_pair(&I, new PointsToRelation()));
            lin.insert(std::make_pair(&I, new std::set<PointsToNode*>()));
            lout.insert(std::make_pair(&I, new std::set<PointsToNode*>()));
        }
    }

    // Create and initialize worklist.
    SmallVector<Instruction *, 128> worklist;
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; I++)
        worklist.push_back(&*I);

    while (!worklist.empty()) {
        Instruction *I = worklist.pop_back_val();

        auto instruction_ain = ain.find(I)->second,
             instruction_aout = aout.find(I)->second;
        auto instruction_lin = lin.find(I)->second,
             instruction_lout = lout.find(I)->second;

        bool addPredsToWorklist = false,
             addSuccsToWorklist = false,
             addCurrToWorklist = false;

        // Compute lout for the current instruction.
        if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
            // If this instruction is a terminator, it may have multiple
            // successors.
            instruction_lout->clear();
            for (unsigned i = 0; i < TI->getNumSuccessors(); i++) {
                Instruction *Succ = TI->getSuccessor(i)->begin();
                auto succ_lin = lin.find(Succ)->second;
                instruction_lout->insert(succ_lin->begin(), succ_lin->end());
            }
        }
        else {
            // If this instruction is not a terminator, it has exactly one
            // successor -- the next instruction in the function.
            Instruction *Succ = getNextInstruction(I);
            auto succ_lin = lin.find(Succ)->second;
            if (*succ_lin != *instruction_lout) {
                instruction_lout->clear();
                instruction_lout->insert(succ_lin->begin(), succ_lin->end());
            }
        }

        // Compute lin for the current instruction.
        std::set<PointsToNode *> n;
        n.insert(instruction_lout->begin(), instruction_lout->end());
        subtractKill(n, I, instruction_ain);
        unionRef(n, I, instruction_lout, instruction_ain);
        // If the two sets are the same, then no changes need to be made to lin,
        // so don't do anything here. Otherwise, we need to update lin and add
        // the predecessors of the current instruction to the worklist.
        if (n != *instruction_lin) {
            instruction_lin->clear();
            instruction_lin->insert(n.begin(), n.end());
            addPredsToWorklist = true;
        }

        // Compute ain for the current instruction.
        PointsToRelation s;
        if (I == &*inst_begin(F)) {
            // If this is the first instruction of the function, then we don't
            // know what anything points to yet.
            for (PointsToNode *N : *instruction_lin) {
                std::pair<PointsToNode *, PointsToNode *> p =
                    std::make_pair(N, factory.getUnknown());
                s.insert(p);
            }
        }
        else {
            // If this is not the first instruction, then the points to
            // information from the predecessors can be propagated forwards.
            BasicBlock *BB = I->getParent();
            Instruction *FirstInBB = BB->begin();
            if (FirstInBB == I) {
                for (pred_iterator PI = pred_begin(BB), E = pred_end(BB);
                     PI != E;
                     ++PI) {
                    BasicBlock *PredBB = *PI;
                    Instruction *Pred = --(PredBB->end());
                    PointsToRelation *PredAout = aout.find(Pred)->second;
                    unionRelationRestriction(s, PredAout, instruction_lin);
                }
            }
            else {
                Instruction *Pred = getPreviousInstruction(I);
                PointsToRelation *PredAout = aout.find(Pred)->second;
                unionRelationRestriction(s, PredAout, instruction_lin);
            }
        }
        if (s != *instruction_ain) {
            instruction_ain->clear();
            instruction_ain->insert(s.begin(), s.end());
            addCurrToWorklist = true;
        }

        // Compute aout for the current instruction.
        s = PointsToRelation();
        std::set<PointsToNode *> notKilled = *instruction_lout;
        subtractKill(notKilled, I, instruction_ain);
        unionRelationRestriction(s, instruction_ain, &notKilled);
        std::set<PointsToNode *> def =
            getRestrictedDef(I, instruction_ain, instruction_lout);
        std::set<PointsToNode *> pointee = getPointee(I, instruction_ain);
        unionCrossProduct(s, def, pointee);
        if (s != *instruction_aout) {
            instruction_aout->clear();
            instruction_aout->insert(s.begin(), s.end());
            addSuccsToWorklist = true;
        }

        // Add succs to worklist
        if (addSuccsToWorklist) {
            if (TerminatorInst *TI = dyn_cast<TerminatorInst>(I)) {
                for (unsigned i = 0; i < TI->getNumSuccessors(); i++)
                    worklist.push_back(TI->getSuccessor(i)->begin());
            }
            else
                worklist.push_back(getNextInstruction(I));
        }

        // Add current instruction to worklist
        if (addCurrToWorklist)
            worklist.push_back(I);

        // Add preds to worklist
        if (addPredsToWorklist) {
            BasicBlock *BB = I->getParent();
            Instruction *FirstInBB = BB->begin();
            if (FirstInBB == I) {
                for (pred_iterator PI = pred_begin(BB), E = pred_end(BB);
                     PI != E;
                     ++PI) {
                    BasicBlock *Pred = *PI;
                    worklist.push_back(--(Pred->end()));
                }
            }
            else
                worklist.push_back(getPreviousInstruction(I));
        }
    }

    for (auto &KV : ain)
        pointsto.insert(KV);
}

std::set<std::pair<PointsToNode *, PointsToNode *>>*
IntraproceduralLivenessPointsTo::getPointsTo(Instruction &I) const {
    return pointsto.find(&I)->second;
}
