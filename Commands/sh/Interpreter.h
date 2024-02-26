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
#include "ShellContext.h"
#include "StackAllocator.h"

typedef struct Interpreter {
    StackAllocatorRef _Nonnull  allocator;
    char* _Nonnull              pathBuffer; // Buffer big enough to hold one absolute path of max length
    ShellContextRef _Weak       context;
} Interpreter;
typedef Interpreter* InterpreterRef;


extern errno_t Interpreter_Create(ShellContextRef _Nonnull pContext, InterpreterRef _Nullable * _Nonnull pOutSelf);
extern void Interpreter_Destroy(InterpreterRef _Nullable self);

// Interprets 'pScript' and executes all its statements.
extern void Interpreter_Execute(InterpreterRef _Nonnull self, ScriptRef _Nonnull pScript);

#endif  /* Interpreter_h */
