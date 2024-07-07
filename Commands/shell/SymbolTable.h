//
//  SymbolTable.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef SymbolTable_h
#define SymbolTable_h

#include <System/System.h>
#include <stdbool.h>

struct ShellContext;


typedef int (*CommandCallback)(struct ShellContext* _Nonnull, int argc, char** argv);

typedef struct Command {
    CommandCallback _Nonnull    cb;
} Command;


typedef enum VariableType {
    kVariableType_String = 0,
} VariableType;

enum {
    kSymbolFlag_Mutable = 1,
    kSymbolFlag_Exported = 2,       // should be included in a command's environment variables
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
} Scope;

typedef struct SymbolTable {
    Scope* _Nonnull currentScope;
} SymbolTable;


extern errno_t SymbolTable_Create(SymbolTable* _Nullable * _Nonnull pOutSelf);
extern void SymbolTable_Destroy(SymbolTable* _Nullable self);

extern errno_t SymbolTable_PushScope(SymbolTable* _Nonnull self);
extern errno_t SymbolTable_PopScope(SymbolTable* _Nonnull self);

extern Symbol* _Nullable SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name);

extern errno_t SymbolTable_AddCommand(SymbolTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb);
extern errno_t SymbolTable_AddVariable(SymbolTable* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags);

#endif  /* SymbolTable_h */
