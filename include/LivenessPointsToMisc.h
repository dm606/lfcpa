#ifndef LFCPA_LIVENESSPOINTSTOMISC_H
#define LFCPA_LIVENESSPOINTSTOMISC_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/BasicBlock.h"

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

#endif
