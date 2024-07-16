//
//  NameTable.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef NameTable_h
#define NameTable_h

#include "Errors.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct ShellContext;


typedef int (*CommandCallback)(struct ShellContext* _Nonnull, int argc, char** argv, char** envp);


typedef struct Symbol {
    struct Symbol* _Nullable    next;       // Next symbol in hash chain
    CommandCallback _Nonnull    cb;
    char                        name[1];
} Symbol;


typedef struct Namespace {
    struct Namespace* _Nullable parent;
    Symbol **                   hashtable;
    size_t                      hashtableCapacity;
    int                         level;
} Namespace;

typedef struct NameTable {
    Namespace* _Nonnull currentNamespace;
} NameTable;


typedef errno_t (*NameTableIterator)(void* _Nullable context, const Symbol* _Nonnull symbol, int level, bool* _Nonnull pOutDone);


extern errno_t NameTable_Create(NameTable* _Nullable * _Nonnull pOutSelf);
extern void NameTable_Destroy(NameTable* _Nullable self);

extern errno_t NameTable_PushNamespace(NameTable* _Nonnull self);
extern errno_t NameTable_PopNamespace(NameTable* _Nonnull self);

// Looks through the namespaces on the namespace stack and returns the top-most
// definition of the symbol with name 'name'.
extern Symbol* _Nullable NameTable_GetSymbol(NameTable* _Nonnull self, const char* _Nonnull name);

// Iterates all symbol definitions. Note that this includes symbols in a lower
// namespace scope that are shadowed in a higher namespace scope. The callback
// has to resolve this ambiguity itself. It may use the provided scope level
// to do this. That said, this function guarantees that symbols are iterated
// starting in the current scope and moving towards the bottom scope. It also
// guarantees that all symbols of a scope X are iterated before the symbols of
// the parent scope X-1 are iterated. The iteration continues until the callback
// either returns with an error or it sets 'pOutDone' to true. Note that
// 'pOutDone' is initialized to false when the iterator is called.
extern errno_t NameTable_IterateSymbols(NameTable* _Nonnull self, NameTableIterator _Nonnull cb, void* _Nullable context);

extern errno_t NameTable_AddSymbol(NameTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb);

#endif  /* NameTable_h */
