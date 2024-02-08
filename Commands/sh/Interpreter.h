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
    StackAllocatorRef _Nonnull  allocator;
    char* _Nonnull              pathBuffer; // Buffer big enough to hold one absolute path of max length
} Interpreter;
typedef Interpreter* InterpreterRef;


typedef int (*InterpreterCommandCallback)(InterpreterRef _Nonnull, int argc, char** argv);

typedef struct InterpreterCommand {
    const char*                 name;
    InterpreterCommandCallback  cb;
} InterpreterCommand;


typedef errno_t (*DirectoryIteratorCallback)(InterpreterRef _Nonnull self, const char* _Nonnull pDirPath, struct _directory_entry_t* _Nonnull pEntry, void* _Nullable pContext);

struct DirectoryEntryFormat {
    int linkCountWidth;
    int uidWidth;
    int gidWidth;
    int sizeWidth;
    int inodeIdWidth;
};


extern errno_t Interpreter_Create(InterpreterRef _Nullable * _Nonnull pOutInterpreter);
extern void Interpreter_Destroy(InterpreterRef _Nullable self);

// Interprets 'pScript' and executes all its statements.
extern void Interpreter_Execute(InterpreterRef _Nonnull self, ScriptRef _Nonnull pScript);

#endif  /* Interpreter_h */
