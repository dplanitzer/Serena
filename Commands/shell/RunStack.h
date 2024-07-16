//
//  RunStack.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef RunStack_h
#define RunStack_h

#include "Errors.h"
#include "Variable.h"


typedef struct NamedVariable {
    struct NamedVariable* _Nullable next;       // Next variable in hash chain
    const char* _Nonnull            name;
    Variable                        var;
} NamedVariable;


typedef struct Scope {
    struct Scope* _Nullable parentScope;
    NamedVariable **        hashtable;
    size_t                  hashtableCapacity;
    int                     level;                      // Scope level. The first level (global scope) is 0, next inner scope is 1, etc...
    int                     exportedVariablesCount;     // Number of exported variable definitions in this scope
} Scope;


typedef struct RunStack {
    Scope* _Nonnull currentScope;
    int             exportedVariablesGeneration;
} RunStack;


typedef errno_t (*RunStackIterator)(void* _Nullable context, const Variable* _Nonnull variable, const char* _Nonnull name, int scopeLevel, bool* _Nonnull pOutDone);


extern errno_t RunStack_Create(RunStack* _Nullable * _Nonnull pOutSelf);
extern void RunStack_Destroy(RunStack* _Nullable self);

extern errno_t RunStack_PushScope(RunStack* _Nonnull self);
extern errno_t RunStack_PopScope(RunStack* _Nonnull self);

extern errno_t RunStack_SetVariableExported(RunStack* _Nonnull self, const char* _Nonnull name, bool bExported);

// Returns a number that represents the current generation of exported variables.
// This number changes every time a new exported variable is added to the current
// scope, a variable is exported or no longer exported or the current scope is
// popped off the scope stack and it contained exported variables.
extern int RunStack_GetExportedVariablesGeneration(RunStack* _Nonnull self);

// Looks through the scopes on the scope stack and returns the top-most definition
// of the symbol with name 'name' and type 'type'.
extern Variable* _Nullable RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nonnull name);

// Iterates all definitions of the given symbol type. Note that this includes
// symbols in a lower scope that are shadowed in a higher scope. The callback
// has to resolve this ambiguity itself. It may use the provided scope level
// to do this. That said, this function guarantees that symbols are iterated
// starting in the current scope and moving towards the bottom scope. It also
// guarantees that all symbols of a scope X are iterated before the symbols of
// the parent scope X-1 are iterated. The iteration continues until the callback
// either returns with an error or it sets 'pOutDone' to true. Note that
// 'pOutDone' is initialized to false when teh iterator is called.
extern errno_t RunStack_Iterate(RunStack* _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context);

extern errno_t RunStack_AddVariable(RunStack* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags);

#endif  /* RunStack_h */
