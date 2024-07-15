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
#include "ArgumentVector.h"
#include "EnvironCache.h"
#include "ShellContext.h"
#include "StackAllocator.h"
#include "SymbolTable.h"


typedef struct Interpreter {
    StackAllocatorRef _Nonnull  allocator;
    
    ShellContextRef _Weak       context;
    SymbolTable* _Nonnull       symbolTable;
    EnvironCache* _Nonnull      environCache;
    ArgumentVector* _Nonnull    argumentVector;
} Interpreter;
typedef Interpreter* InterpreterRef;


extern errno_t Interpreter_Create(ShellContextRef _Nonnull pContext, InterpreterRef _Nullable * _Nonnull pOutSelf);
extern void Interpreter_Destroy(InterpreterRef _Nullable self);

// Interprets 'pScript' and executes all its statements.
extern void Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script);

#endif  /* Interpreter_h */
