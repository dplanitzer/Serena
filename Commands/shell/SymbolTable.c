//
//  SymbolTable.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "SymbolTable.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


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
    self->hashtable = 0;
    self->hashtableCapacity = 0;
}

static size_t hash_cstring(const char* _Nonnull str)
{
    size_t h = 5381;
    char c;

    while (c = *str++) {
        h = ((h << 5) + h) + c;
    }

    return h;
}

static Symbol* _Nullable _Scope_GetSymbol(Scope* _Nonnull self, SymbolType type, const char* _Nonnull name, size_t hashIdx)
{
    Symbol* sym = self->hashtable[hashIdx];

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
    return _Scope_GetSymbol(self, kSymbolType_Command, name, hash_cstring(name) % self->hashtableCapacity);
}

static errno_t Scope_AddCommand(Scope* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    decl_try_err();
    Symbol* sym;
    const size_t hashCode = hash_cstring(name) % self->hashtableCapacity;

    if (_Scope_GetSymbol(self, kSymbolType_Command, name, hashCode)) {
        throw(EEXIST);
    }

    try(Symbol_CreateCommand(name, cb, &sym));
    sym->next = self->hashtable[hashCode];
    self->hashtable[hashCode] = sym;

catch:
    return err;
}

static errno_t Scope_AddVariable(Scope* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    decl_try_err();
    Symbol* sym;
    const size_t hashCode = hash_cstring(name) % self->hashtableCapacity;

    if (_Scope_GetSymbol(self, kSymbolType_Variable, name, hashCode)) {
        throw(EEXIST);
    }

    try(Symbol_CreateStringVariable(name, value, flags, &sym));
    sym->next = self->hashtable[hashCode];
    self->hashtable[hashCode] = sym;

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
        return EOVERFLOW;
    }

    Scope* scope = self->currentScope;
    self->currentScope = scope->parentScope;
    scope->parentScope = NULL;
    Scope_Destroy(scope);

    return EOK;
}

Symbol* _Nullable SymbolTable_GetSymbol(SymbolTable* _Nonnull self, SymbolType type, const char* _Nonnull name)
{
    return Scope_GetSymbol(self->currentScope, type, name);
}

errno_t SymbolTable_AddCommand(SymbolTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    return Scope_AddCommand(self->currentScope, name, cb);
}

errno_t SymbolTable_AddVariable(SymbolTable* _Nonnull self, const char* _Nonnull name, const char* _Nonnull value, unsigned int flags)
{
    return Scope_AddVariable(self->currentScope, name, value, flags);
}