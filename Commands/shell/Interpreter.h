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
#include "LineReader.h"
#include "NameTable.h"
#include "OpStack.h"
#include "RunStack.h"
#include "StackAllocator.h"


typedef enum ExecuteOptions {
    kExecute_PushScope = 1,     // Push a scope before running the script and pop it afterwards
    kExecute_Interactive = 2,   // Interactive mode. I.e. print the result of (the last expression of) the script
} ExecuteOptions;


typedef struct CDEntry {
    struct CDEntry* _Nullable   prev;
    char* _Nonnull              path;
} CDEntry;


typedef struct Interpreter {
    StackAllocatorRef _Nonnull  allocator;
    
    LineReaderRef _Weak         lineReader;
    NameTable* _Nonnull         nameTable;
    OpStack* _Nonnull           opStack;
    RunStack* _Nonnull          runStack;
    EnvironCache* _Nonnull      environCache;
    ArgumentVector* _Nonnull    argumentVector;
    CDEntry* _Nullable          cdStackTos;

    int32_t                     loopNestingCount;
    bool                        isInteractive;
} Interpreter;
typedef Interpreter* InterpreterRef;


extern InterpreterRef _Nonnull Interpreter_Create(LineReaderRef _Nonnull lineReader);
extern void Interpreter_Destroy(InterpreterRef _Nullable self);

// Interprets 'pScript' and executes all its statements.
extern errno_t Interpreter_Execute(InterpreterRef _Nonnull self, Script* _Nonnull script, ExecuteOptions options);

//
// For use by command callbacks
//

extern errno_t Interpreter_IterateVariables(InterpreterRef _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context);

extern int Interpreter_GetHistoryCount(InterpreterRef _Nonnull self);
extern const char* _Nonnull Interpreter_GetHistoryAt(InterpreterRef _Nonnull self, int idx);

#endif  /* Interpreter_h */
