//
//  RunStack.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "RunStack.h"
#include "Utilities.h"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////

static void Variable_Destroy(Variable* _Nullable self);

static errno_t Variable_Create(unsigned int modifiers, const char* _Nonnull name, const Value* _Nonnull value, Variable* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Variable* self;

    try_null(self, calloc(1, sizeof(Variable)), errno);
    try_null(self->name, strdup(name), errno);
    Value_InitCopy(&self->value, value);
    self->modifiers = modifiers;

    *pOutSelf = self;
    return EOK;

catch:
    Variable_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Variable_Destroy(Variable* _Nullable self)
{
    if (self) {
        Value_Deinit(&self->value);

        free(self->name);
        self->name = NULL;

        free(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Scope
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_HASHTABLE_CAPACITY  16

static void Scope_Destroy(Scope* _Nullable self);
static void _Scope_DestroyHashtable(Scope* _Nonnull self);


static errno_t Scope_Create(Scope* _Nullable parentScope, Scope* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Scope* self;

    try_null(self, calloc(1, sizeof(Scope)), errno);
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Variable*)), errno);
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    self->parent = parentScope;
    self->level = (parentScope) ? parentScope->level + 1 : 0;

    *pOutSelf = self;
    return EOK;

catch:
    Scope_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Scope_Destroy(Scope* _Nullable self)
{
    if (self) {
        _Scope_DestroyHashtable(self);
        free(self);
    }
}

static void _Scope_DestroyHashtable(Scope* _Nonnull self)
{
    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        Variable* vp = self->hashtable[i];

        while (vp) {
            Variable* next_vp = vp->next;

            Variable_Destroy(vp);
            vp = next_vp;
        }

        self->hashtable[i] = NULL;
    }

    free(self->hashtable);
    self->hashtable = NULL;
    self->hashtableCapacity = 0;
}

static Variable* _Nullable _Scope_GetVariable(Scope* _Nonnull self, const char* _Nonnull name, size_t hashCode)
{
    Variable* vp = self->hashtable[hashCode % self->hashtableCapacity];

    while (vp) {
        if (!strcmp(vp->name, name)) {
            return vp;
        }

        vp = vp->next;
    }

    return NULL;
}

static Variable* _Nullable Scope_GetVariable(Scope* _Nonnull self, const char* _Nonnull name)
{
    return _Scope_GetVariable(self, name, hash_cstring(name));
}

static errno_t Scope_Iterate(Scope* _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context, bool* _Nonnull pOutDone)
{
    decl_try_err();
    bool done = false;

    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        Variable* vp = self->hashtable[i];

        while (vp) {
            err = cb(context, vp, self->level, &done);

            if (err != EOK || done) {
                break;
            }
        
            vp = vp->next;
        }
    }

    *pOutDone = done;
    return err;
}

static errno_t Scope_DeclareVariable(Scope* _Nonnull self, unsigned int modifiers, const char* _Nonnull name, const Value* value)
{
    decl_try_err();
    Variable* vp;
    const size_t hashCode = hash_cstring(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Scope_GetVariable(self, name, hashCode)) {
        throw(EREDEFINED);
    }

    try(Variable_Create(modifiers, name, value, &vp));
    vp->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = vp;

catch:
    return err;
}



////////////////////////////////////////////////////////////////////////////////
// RunStack
////////////////////////////////////////////////////////////////////////////////

errno_t RunStack_Create(RunStack* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    RunStack* self;

    try_null(self, calloc(1, sizeof(RunStack)), errno);
    try(RunStack_PushScope(self));
    
    *pOutSelf = self;
    return EOK;

catch:
    RunStack_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void RunStack_Destroy(RunStack* _Nullable self)
{
    if (self) {
        Scope* scope = self->currentScope;

        while (scope) {
            Scope* next_scope = scope->parent;

            Scope_Destroy(scope);
            scope = next_scope;
        }
        self->currentScope = NULL;

        free(self);
    }
}

errno_t RunStack_PushScope(RunStack* _Nonnull self)
{
    decl_try_err();
    Scope* scope;

    try(Scope_Create(self->currentScope, &scope));
    if (self->currentScope == NULL) {
        self->globalScope = scope;
    }
    else if (self->currentScope == self->globalScope) {
        self->scriptScope = scope;
    }
    self->currentScope = scope;

catch:
    return err;
}

errno_t RunStack_PopScope(RunStack* _Nonnull self)
{
    if (self->currentScope->parent == NULL) {
        return EUNDERFLOW;
    }

    if (self->currentScope->publicVariablesCount > 0) {
        self->generationOfPublicVariables++;
    }

    Scope* scope = self->currentScope;
    if (scope == self->scriptScope) {
        self->scriptScope = NULL;
    }
    else if (scope == self->globalScope) {
        self->globalScope = NULL;
    }
    self->currentScope = scope->parent;
    scope->parent = NULL;
    Scope_Destroy(scope);

    return EOK;
}

static Scope* _Nullable _RunStack_GetScopeForName(RunStack* _Nonnull self, const char* _Nullable scopeName)
{
    Scope* scope = self->currentScope;

    if (scopeName && *scopeName != '\0') {
        if (!strcmp(scopeName, "global")) {
            scope = self->globalScope;
        }
        else if (!strcmp(scopeName, "script")) {
            scope = self->scriptScope;
        }
        else if (!strcmp(scopeName, "local")) {
            scope = self->currentScope;
        }
        else {
            scope = NULL;
        }
    }
    return scope;
}

static Variable* _Nullable _RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nullable scopeName, const char* _Nonnull name, Scope* _Nonnull * _Nullable pOutScope)
{
    const size_t hashCode = hash_cstring(name);
    Scope* scope = _RunStack_GetScopeForName(self, scopeName);
    Variable* vp = NULL;

    while (scope) {
        vp = _Scope_GetVariable(scope, name, hashCode);
        if (vp) {
            break;
        }

        scope = scope->parent;
    }

    if (pOutScope) {
        *pOutScope = (vp) ? scope : NULL;
    }

    return vp;
}

errno_t RunStack_SetVariablePublic(RunStack* _Nonnull self, const char* _Nullable scopeName, const char* _Nonnull name, bool bPublic)
{
    Scope* scope;
    Variable* vp = _RunStack_GetVariable(self, scopeName, name, &scope);

    if (vp == NULL) {
        return (scope) ? EUNDEFINED : ENOSCOPE;
    }

    if (bPublic && (vp->modifiers & kVarModifier_Public) == 0) {
        vp->modifiers |= kVarModifier_Public;
        scope->publicVariablesCount++;
        self->generationOfPublicVariables++;
    }
    else if (!bPublic && (vp->modifiers & kVarModifier_Public) == kVarModifier_Public) {
        vp->modifiers &= ~kVarModifier_Public;
        scope->publicVariablesCount--;
        self->generationOfPublicVariables++;
    }

    return EOK;
}

int RunStack_GetGenerationOfPublicVariables(RunStack* _Nonnull self)
{
    return self->generationOfPublicVariables;
}

Variable* _Nullable RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nullable scopeName, const char* _Nonnull name)
{
    return _RunStack_GetVariable(self, scopeName, name, NULL);
}

errno_t RunStack_Iterate(RunStack* _Nonnull self, RunStackIterator _Nonnull cb, void* _Nullable context)
{
    decl_try_err();
    bool done = false;
    Scope* scope = self->currentScope;

    while (scope) {
        err = Scope_Iterate(scope, cb, context, &done);

        if (err != EOK || done) {
            break;
        }

        scope = scope->parent;
    }

    return err;
}

errno_t RunStack_DeclareVariable(RunStack* _Nonnull self, unsigned int modifiers, const char* _Nullable scopeName, const char* _Nonnull name, const Value* _Nonnull value)
{
    decl_try_err();
    Scope* scope = _RunStack_GetScopeForName(self, scopeName);

    if (scope == NULL) {
        return ENOSCOPE;
    }

    err = Scope_DeclareVariable(self->currentScope, modifiers, name, value);
    if (err == EOK && (modifiers & kVarModifier_Public) == kVarModifier_Public) {
        self->currentScope->publicVariablesCount++;
        self->generationOfPublicVariables++;
    }

    return err;
}