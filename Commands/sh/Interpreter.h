//
//  Interpreter.h
//  sh
//
//  Created by Dietmar Planitzer on 1/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Interpreter_h
#define Interpreter_h

#include "Script.h"
#include "StackAllocator.h"

typedef struct Interpreter {
    StackAllocatorRef allocator;
} Interpreter;
typedef Interpreter* InterpreterRef;


extern errno_t Interpreter_Create(InterpreterRef _Nullable * _Nonnull pOutInterpreter);
extern void Interpreter_Destroy(InterpreterRef _Nullable self);

// Interprets 'pScript' and executes all its statements.
extern void Interpreter_Execute(InterpreterRef _Nonnull self, ScriptRef _Nonnull pScript);

#endif  /* Interpreter_h */
