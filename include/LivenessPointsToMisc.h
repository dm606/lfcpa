#ifndef LFCPA_LIVENESSPOINTSTOMISC_H
#define LFCPA_LIVENESSPOINTSTOMISC_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"

using namespace llvm;

inline Instruction *getNextInstruction(Instruction *Instr)  {
    BasicBlock::iterator I(Instr);
    Instruction *Next = ++I;
    // There should be at least one more instruction in the basic block.
    assert(Next != Instr->getParent()->end());
    return Next;
}

inline Instruction *getPreviousInstruction(Instruction *Instr) {
    BasicBlock::iterator I(Instr);
    // There should be at least one instruction before Instr.
    assert (I != Instr->getParent()->begin());
    return --I;
}

#endif
