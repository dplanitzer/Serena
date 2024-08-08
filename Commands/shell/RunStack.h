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
#include "Value.h"


enum {
    kVarModifier_Mutable = 1,
    kVarModifier_Public = 2,      // Should be included in a command's environment Values
};


typedef struct Variable {
    struct Variable* _Nullable  next;       // Next variable in hash chain
    const char* _Nonnull        name;
    uint32_t                    modifiers;
    Value                       value;
} Variable;


typedef struct Scope {
    struct Scope* _Nullable parent;
    Variable **             hashtable;
    size_t                  hashtableCapacity;
    int                     level;                      // Scope level. The first level (global scope) is 0, next inner scope is 1, etc...
    int                     publicVariablesCount;       // Number of public variable definitions in this scope
} Scope;


typedef struct RunStack {
    Scope* _Nonnull     currentScope;
    Scope* _Nonnull     globalScope;
    Scope* _Nullable    scriptScope;
    int                 generationOfPublicVariables;

    Scope* _Nullable    cachedScopes;                   // Threaded through scope->parent
    int                 cachedScopesCount;
} RunStack;


typedef errno_t (*RunStackIterator)(void* _Nullable context, const Variable* _Nonnull value, int level, bool* _Nonnull pOutDone);


extern errno_t RunStack_Create(RunStack* _Nullable * _Nonnull pOutSelf);
extern void RunStack_Destroy(RunStack* _Nullable self);

extern errno_t RunStack_PushScope(RunStack* _Nonnull self);
extern errno_t RunStack_PopScope(RunStack* _Nonnull self);

// Marks the variable named 'name' as public or internal. Public variables are
// exported to the environment and thus are visible to child processes; internal
// variables are only visible to the shell process.
extern errno_t RunStack_SetVariablePublic(RunStack* _Nonnull self, const char* _Nullable scope, const char* _Nonnull name, bool bPublic);

// Returns a number that represents the current generation of public variables.
// This number changes every time a new public variable is added to the current
// scope, a variable is public or no longer public or the current scope is
// popped off the scope stack and it contained public variables.
extern int RunStack_GetGenerationOfPublicVariables(RunStack* _Nonnull self);

// Looks through the scopes on the run stack and returns the top-most definition
// of the variable with name 'name'. 'scope' my be 'global', 'script' or 'local'
// (or "").
extern Variable* _Nullable RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nullable scope, const char* _Nonnull name);

// Iterates all variable definitions. Note that this includes variables in a
// lower scope that are shadowed in a higher scope. The callback has to resolve
// this ambiguity itself. It may use the provided scope level to do this. That
// said, this function guarantees that variables are iterated starting in the
// current scope and moving towards the bottom scope. It also guarantees that
// all variables of a scope X are iterated before the variable of the parent
// scope X-1 are iterated. The iteration continues until the callback either
// returns with an error or it sets 'pOutDone' to true. Note that 'pOutDone' is
// initialized to false when the iterator is called.
extern errno_t RunStack_Iterate(RunStack* _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context);

extern errno_t RunStack_DeclareVariable(RunStack* _Nonnull self, unsigned int modifiers, const char* _Nullable scope, const char* _Nonnull name, const Value* _Nonnull value);

#endif  /* RunStack_h */
