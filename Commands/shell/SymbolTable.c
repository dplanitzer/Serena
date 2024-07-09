//
//  SymbolTable.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "SymbolTable.h"
#include "builtins/cmdlib.h"
#include <string.h>

static Symbol* _Nullable _SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name, Scope* _Nonnull * _Nullable);


////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////

// Returns the length of the string that represents the value of the variable.
size_t Variable_GetStringValueLength(const Variable* _Nonnull self)
{
    switch (self->type) {
        case kVariableType_String:
            return self->u.string.length;

        default:
            return 0;
    }
}

// Copies up to 'bufSize' - 1 characters of the variable's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
bool Variable_GetStringValue(const Variable* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    bool copiedAll = false;

    if (bufSize < 1) {
        return false;
    }

    switch (self->type) {
        case kVariableType_String: {
            char* src = self->u.string.characters;

            while (bufSize > 1 && *src != '\0') {
                *buf++ = *src++;
                bufSize--;
            }
            *buf = '\0';
            copiedAll = *src == '\0';
            break;
        }

        default:
            *buf = '\0';
            break;
    }

    return copiedAll;
}


////////////////////////////////////////////////////////////////////////////////
// Symbol
////////////////////////////////////////////////////////////////////////////////

static void Symbol_Destroy(Symbol* _Nullable self);

static errno_t Symbol_CreateCommand(const char* _Nonnull name, CommandCallback _Nonnull cb, Symbol* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Symbol* self;

    try_null(self, calloc(1, sizeof(Symbol)), errno);
    try_null(self->name, strdup(name), errno);
    self->type = kSymbolType_Command;
    self->u.command.cb = cb;

    *pOutSelf = self;
    return EOK;

catch:
    Symbol_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static errno_t Symbol_CreateStringVariable(const char* _Nonnull name, const char* value, unsigned int flags, Symbol* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Symbol* self;

    try_null(self, calloc(1, sizeof(Symbol)), errno);
    try_null(self->name, strdup(name), errno);
    self->type = kSymbolType_Variable;
    self->u.variable.type = kVariableType_String;
    self->u.variable.flags = flags;
    try_null(self->u.variable.u.string.characters, strdup(value), errno);
    self->u.variable.u.string.length = strlen(value);

    *pOutSelf = self;
    return EOK;

catch:
    Symbol_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Symbol_Destroy(Symbol* _Nullable self)
{
    if (self) {
        switch (self->type) {
            case kSymbolType_Variable:
                free(self->u.variable.u.string.characters);
                self->u.variable.u.string.characters = NULL;
                self->u.variable.u.string.length = 0;
                break;

            default:
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
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Symbol*)), errno);
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
        Symbol* sym = self->hashtable[i];

        while (sym) {
            Symbol* next_sym = sym->next;

            Symbol_Destroy(sym);
            sym = next_sym;
        }

        self->hashtable[i] = NULL;
    }

    free(self->hashtable);
    self->hashtable = NULL;
    self->hashtableCapacity = 0;
}

static Symbol* _Nullable _Scope_GetSymbol(Scope* _Nonnull self, SymbolType type, const char* _Nonnull name, size_t hashCode)
{
    Symbol* sym = self->hashtable[hashCode % self->hashtableCapacity];

    while (sym) {
        if (sym->type == type && !strcmp(sym->name, name)) {
            return sym;
        }

        sym = sym->next;
    }

    return NULL;
}

static Symbol* _Nullable Scope_GetSymbol(Scope* _Nonnull self, SymbolType type, const char* _Nonnull name)
{
    return _Scope_GetSymbol(self, kSymbolType_Command, name, hash_cstring(name));
}

static errno_t Scope_IterateSymbols(Scope* _Nonnull self, SymbolTableIterator _Nonnull cb, void* _Nullable context, bool* _Nonnull pOutDone)
{
    decl_try_err();
    bool done = false;

    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        Symbol* sym = self->hashtable[i];

        while (sym) {
            err = cb(context, sym, self->level, &done);

            if (err != EOK || done) {
                break;
            }
        
            sym = sym->next;
        }
    }

    *pOutDone = done;
    return err;
}

static errno_t Scope_AddCommand(Scope* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    decl_try_err();
    Symbol* sym;
    const size_t hashCode = hash_cstring(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Scope_GetSymbol(self, kSymbolType_Command, name, hashCode)) {
        throw(EREDEFINED);
    }

    try(Symbol_CreateCommand(name, cb, &sym));
    sym->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = sym;

catch:
    return err;
}

static errno_t Scope_AddVariable(Scope* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    decl_try_err();
    Symbol* sym;
    const size_t hashCode = hash_cstring(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Scope_GetSymbol(self, kSymbolType_Variable, name, hashCode)) {
        throw(EREDEFINED);
    }

    try(Symbol_CreateStringVariable(name, value, flags, &sym));
    sym->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = sym;

catch:
    return err;
}



////////////////////////////////////////////////////////////////////////////////
// SymbolTable
////////////////////////////////////////////////////////////////////////////////

errno_t SymbolTable_Create(SymbolTable* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    SymbolTable* self;

    try_null(self, calloc(1, sizeof(SymbolTable)), errno);
    try(SymbolTable_PushScope(self));
    
    *pOutSelf = self;
    return EOK;

catch:
    SymbolTable_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void SymbolTable_Destroy(SymbolTable* _Nullable self)
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

errno_t SymbolTable_PushScope(SymbolTable* _Nonnull self)
{
    decl_try_err();
    Scope* scope;

    try(Scope_Create(self->currentScope, &scope));
    self->currentScope = scope;

catch:
    return err;
}

errno_t SymbolTable_PopScope(SymbolTable* _Nonnull self)
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

errno_t SymbolTable_SetVariableExported(SymbolTable* _Nonnull self, const char* _Nonnull name, bool bExported)
{
    Scope* scope;
    Symbol* sym = _SymbolTable_GetSymbol(self, kSymbolType_Variable, name, &scope);

    if (sym == NULL) {
        return EUNDEFINED;
    }

    if (bExported && (sym->u.variable.flags & kVariableFlag_Exported) == 0) {
        sym->u.variable.flags |= kVariableFlag_Exported;
        scope->exportedVariablesCount++;
        self->exportedVariablesGeneration++;
    }
    else if (!bExported && (sym->u.variable.flags & kVariableFlag_Exported) == kVariableFlag_Exported) {
        sym->u.variable.flags &= ~kVariableFlag_Exported;
        scope->exportedVariablesCount--;
        self->exportedVariablesGeneration++;
    }

    return EOK;
}

int SymbolTable_GetExportedVariablesGeneration(SymbolTable* _Nonnull self)
{
    return self->exportedVariablesGeneration;
}

static Symbol* _Nullable _SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name, Scope* _Nonnull * _Nullable pOutScope)
{
    const size_t hashCode = hash_cstring(name);
    Scope* scope = self->currentScope;

    while (scope) {
        Symbol* sym = _Scope_GetSymbol(scope, type, name, hashCode);

        if (sym) {
            if (pOutScope) *pOutScope = scope;
            return sym;
        }

        scope = scope->parentScope;
    }

    if (pOutScope) *pOutScope = NULL;
    return NULL;
}

Symbol* _Nullable SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name)
{
    return _SymbolTable_GetSymbol(self, type, name, NULL);
}

errno_t SymbolTable_IterateSymbols(SymbolTable* _Nonnull self, SymbolTableIterator _Nonnull cb, void* _Nullable context)
{
    decl_try_err();
    bool done = false;
    Scope* scope = self->currentScope;

    while (scope) {
        err = Scope_IterateSymbols(scope, cb, context, &done);

        if (err != EOK || done) {
            break;
        }

        scope = scope->parentScope;
    }

    return err;
}

errno_t SymbolTable_AddCommand(SymbolTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    return Scope_AddCommand(self->currentScope, name, cb);
}

errno_t SymbolTable_AddVariable(SymbolTable* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    decl_try_err();

    err = Scope_AddVariable(self->currentScope, name, value, flags);
    if (err == EOK && (flags & kVariableFlag_Exported) == kVariableFlag_Exported) {
        self->currentScope->exportedVariablesCount++;
        self->exportedVariablesGeneration++;
    }

    return err;
}