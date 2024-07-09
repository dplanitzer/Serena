//
//  SymbolTable.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef SymbolTable_h
#define SymbolTable_h

#include "Errors.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct ShellContext;


typedef int (*CommandCallback)(struct ShellContext* _Nonnull, int argc, char** argv, char** envp);

typedef struct Command {
    CommandCallback _Nonnull    cb;
} Command;


typedef enum VariableType {
    kVariableType_String = 0,
} VariableType;

enum {
    kVariableFlag_Mutable = 1,
    kVariableFlag_Exported = 2,         // Should be included in a command's environment variables
};

typedef struct StringValue {
    char* _Nonnull  characters;
    size_t          length;
} StringValue;

typedef struct Variable {
    int8_t  type;
    uint8_t flags;
    int8_t  reserved[2];
    union {
        StringValue string;
    }       u;
} Variable;

// Returns the length of the string that represents the value of the variable.
extern size_t Variable_GetStringValueLength(const Variable* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the variable's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
extern bool Variable_GetStringValue(const Variable* _Nonnull self, size_t bufSize, char* _Nonnull buf);


typedef enum SymbolType {
    kSymbolType_Variable = 0,
    kSymbolType_Command,
} SymbolType;

typedef struct Symbol {
    struct Symbol* _Nullable    next;       // Next symbol in hash chain
    const char* _Nonnull        name;
    int                         type;
    union {
        Command     command;
        Variable    variable;
    }                           u;
} Symbol;


typedef struct Scope {
    struct Scope* _Nullable parentScope;
    Symbol **               hashtable;
    size_t                  hashtableCapacity;
    int                     level;                      // Scope level. The first level (global scope) is 0, next inner scope is 1, etc...
    int                     exportedVariablesCount;     // Number of exported variable definitions in this scope
} Scope;

typedef struct SymbolTable {
    Scope* _Nonnull currentScope;
    int             exportedVariablesGeneration;
} SymbolTable;


typedef errno_t (*SymbolTableIterator)(void* _Nullable context, const Symbol* _Nonnull symbol, int scopeLevel, bool* _Nonnull pOutDone);


extern errno_t SymbolTable_Create(SymbolTable* _Nullable * _Nonnull pOutSelf);
extern void SymbolTable_Destroy(SymbolTable* _Nullable self);

extern errno_t SymbolTable_PushScope(SymbolTable* _Nonnull self);
extern errno_t SymbolTable_PopScope(SymbolTable* _Nonnull self);

extern errno_t SymbolTable_SetVariableExported(SymbolTable* _Nonnull self, const char* _Nonnull name, bool bExported);

// Returns a number that represents the current generation of exported variables.
// This number changes every time a new exported variable is added to the current
// scope, a variable is exported or no longer exported or the current scope is
// popped off the scope stack and it contained exported variables.
extern int SymbolTable_GetExportedVariablesGeneration(SymbolTable* _Nonnull self);

// Looks through the scopes on the scope stack and returns the top-most definition
// of the symbol with name 'name' and type 'type'.
extern Symbol* _Nullable SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name);

// Iterates all definitions of the given symbol type. Note that this includes
// symbols in a lower scope that are shadowed in a higher scope. The callback
// has to resolve this ambiguity itself. It may use the provided scope level
// to do this. That said, this function guarantees that symbols are iterated
// starting in the current scope and moving towards the bottom scope. It also
// guarantees that all symbols of a scope X are iterated before the symbols of
// the parent scope X-1 are iterated. The iteration continues until the callback
// either returns with an error or it sets 'pOutDone' to true. Note that
// 'pOutDone' is initialized to false when teh iterator is called.
extern errno_t SymbolTable_IterateSymbols(SymbolTable* _Nonnull self, SymbolTableIterator _Nonnull cb, void* _Nullable context);

extern errno_t SymbolTable_AddCommand(SymbolTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb);
extern errno_t SymbolTable_AddVariable(SymbolTable* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags);

#endif  /* SymbolTable_h */
