# Liveness-based pointer analysis for LLVM

Interprocedural alias algorithm for LLVM. Compiling creates a file named `lfcpa.so` containing a pass named `test-pass`. The pass will analyse the IR and print points-to information. Its results can't be used by any transformations yet.

Note: it also currently leaks some memory and some of the code needs refactoring.
