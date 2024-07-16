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
// NamedVariable
////////////////////////////////////////////////////////////////////////////////

static void NamedVariable_Destroy(NamedVariable* _Nullable self);

static errno_t NamedVariable_Create(const char* _Nonnull name, const char* value, unsigned int flags, NamedVariable* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    NamedVariable* self;

    try_null(self, calloc(1, sizeof(NamedVariable)), errno);
    try_null(self->name, strdup(name), errno);
    self->var.type = kVariableType_String;
    self->var.flags = flags;
    try_null(self->var.u.string.characters, strdup(value), errno);
    self->var.u.string.length = strlen(value);

    *pOutSelf = self;
    return EOK;

catch:
    NamedVariable_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void NamedVariable_Destroy(NamedVariable* _Nullable self)
{
    if (self) {
        switch (self->var.type) {
            case kVariableType_String:
                free(self->var.u.string.characters);
                self->var.u.string.characters = NULL;
                self->var.u.string.length = 0;
                break;

            default:
                abort();
                break;
        }

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
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(NamedVariable*)), errno);
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    self->parentScope = parentScope;
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
        NamedVariable* nv = self->hashtable[i];

        while (nv) {
            NamedVariable* next_nv = nv->next;

            NamedVariable_Destroy(nv);
            nv = next_nv;
        }

        self->hashtable[i] = NULL;
    }

    free(self->hashtable);
    self->hashtable = NULL;
    self->hashtableCapacity = 0;
}

static Variable* _Nullable _Scope_GetVariable(Scope* _Nonnull self, const char* _Nonnull name, size_t hashCode)
{
    NamedVariable* nv = self->hashtable[hashCode % self->hashtableCapacity];

    while (nv) {
        if (!strcmp(nv->name, name)) {
            return &nv->var;
        }

        nv = nv->next;
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
        NamedVariable* nv = self->hashtable[i];

        while (nv) {
            err = cb(context, &nv->var, nv->name, self->level, &done);

            if (err != EOK || done) {
                break;
            }
        
            nv = nv->next;
        }
    }

    *pOutDone = done;
    return err;
}

static errno_t Scope_AddVariable(Scope* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    decl_try_err();
    NamedVariable* nv;
    const size_t hashCode = hash_cstring(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Scope_GetVariable(self, name, hashCode)) {
        throw(EREDEFINED);
    }

    try(NamedVariable_Create(name, value, flags, &nv));
    nv->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = nv;

catch:
    return err;
}



////////////////////////////////////////////////////////////////////////////////
// RunStack
////////////////////////////////////////////////////////////////////////////////

static Variable* _Nullable _RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nonnull name, Scope* _Nonnull * _Nullable pOutScope);


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
            Scope* next_scope = scope->parentScope;

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
    self->currentScope = scope;

catch:
    return err;
}

errno_t RunStack_PopScope(RunStack* _Nonnull self)
{
    if (self->currentScope->parentScope == NULL) {
        return EUNDERFLOW;
    }

    if (self->currentScope->exportedVariablesCount > 0) {
        self->exportedVariablesGeneration++;
    }

    Scope* scope = self->currentScope;
    self->currentScope = scope->parentScope;
    scope->parentScope = NULL;
    Scope_Destroy(scope);

    return EOK;
}

errno_t RunStack_SetVariableExported(RunStack* _Nonnull self, const char* _Nonnull name, bool bExported)
{
    Scope* scope;
    Variable* v = _RunStack_GetVariable(self, name, &scope);

    if (v == NULL) {
        return EUNDEFINED;
    }

    if (bExported && (v->flags & kVariableFlag_Exported) == 0) {
        v->flags |= kVariableFlag_Exported;
        scope->exportedVariablesCount++;
        self->exportedVariablesGeneration++;
    }
    else if (!bExported && (v->flags & kVariableFlag_Exported) == kVariableFlag_Exported) {
        v->flags &= ~kVariableFlag_Exported;
        scope->exportedVariablesCount--;
        self->exportedVariablesGeneration++;
    }

    return EOK;
}

int RunStack_GetExportedVariablesGeneration(RunStack* _Nonnull self)
{
    return self->exportedVariablesGeneration;
}

static Variable* _Nullable _RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nonnull name, Scope* _Nonnull * _Nullable pOutScope)
{
    const size_t hashCode = hash_cstring(name);
    Scope* scope = self->currentScope;

    while (scope) {
        Variable* v = _Scope_GetVariable(scope, name, hashCode);

        if (v) {
            if (pOutScope) *pOutScope = scope;
            return v;
        }

        scope = scope->parentScope;
    }

    if (pOutScope) *pOutScope = NULL;
    return NULL;
}

Variable* _Nullable RunStack_GetVariable(RunStack* _Nonnull self, const char* _Nonnull name)
{
    return _RunStack_GetVariable(self, name, NULL);
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

        scope = scope->parentScope;
    }

    return err;
}

errno_t RunStack_AddVariable(RunStack* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    decl_try_err();

    err = Scope_AddVariable(self->currentScope, name, value, flags);
    if (err == EOK && (flags & kVariableFlag_Exported) == kVariableFlag_Exported) {
        self->currentScope->exportedVariablesCount++;
        self->exportedVariablesGeneration++;
    }

    return err;
}