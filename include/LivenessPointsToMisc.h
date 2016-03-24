#ifndef LFCPA_LIVENESSPOINTSTOMISC_H
#define LFCPA_LIVENESSPOINTSTOMISC_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"

using namespace llvm;

inline const Instruction *getNextInstruction(const Instruction *Instr)  {
    BasicBlock::const_iterator I(Instr);
    const Instruction *Next = ++I;
    // There should be at least one more instruction in the basic block.
    assert(Next != Instr->getParent()->end());
    return Next;
}

inline const Instruction *getPreviousInstruction(const Instruction *Instr) {
    BasicBlock::const_iterator I(Instr);
    // There should be at least one instruction before Instr.
    assert (I != Instr->getParent()->begin());
    return --I;
}

inline bool canHandleBitcast(const BitCastInst *I) {
    // We only handle bitcasts intelligently if the bitcast is the only user of
    // the value that is casted.
    if (++(I->getOperand(0)->user_begin()) == I->getOperand(0)->user_end()) {
        assert (*(I->getOperand(0)->user_begin()) == I && "The bitcast should be a user of its operand.");
        auto *Op = I->getOperand(0);
        if (isa<BitCastInst>(Op) || isa<GEPOperator>(Op))
            return false;
        else
            return true;
    }

    return false;
}

#endif
