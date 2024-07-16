//
//  NameTable.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "NameTable.h"
#include "Utilities.h"
#include <string.h>


////////////////////////////////////////////////////////////////////////////////
// Symbol
////////////////////////////////////////////////////////////////////////////////

static void Symbol_Destroy(Symbol* _Nullable self);

static errno_t Symbol_Create(const char* _Nonnull name, CommandCallback _Nonnull cb, Symbol* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    const size_t len = strlen(name);
    Symbol* self;

    try_null(self, calloc(1, sizeof(Symbol) + len), errno);
    memcpy(self->name, name, len + 1);
    self->cb = cb;

    *pOutSelf = self;
    return EOK;

catch:
    Symbol_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Symbol_Destroy(Symbol* _Nullable self)
{
    free(self);
}


////////////////////////////////////////////////////////////////////////////////
// Namespace
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_HASHTABLE_CAPACITY  16

static void Namespace_Destroy(Namespace* _Nullable self);
static void _Namespace_DestroyHashtable(Namespace* _Nonnull self);


static errno_t Namespace_Create(Namespace* _Nullable parentNamespace, Namespace* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Namespace* self;

    try_null(self, calloc(1, sizeof(Namespace)), errno);
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Symbol*)), errno);
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    self->parent = parentNamespace;
    self->level = (parentNamespace) ? parentNamespace->level + 1 : 0;

    *pOutSelf = self;
    return EOK;

catch:
    Namespace_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Namespace_Destroy(Namespace* _Nullable self)
{
    if (self) {
        _Namespace_DestroyHashtable(self);
        free(self);
    }
}

static void _Namespace_DestroyHashtable(Namespace* _Nonnull self)
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

static Symbol* _Nullable _Namespace_GetSymbol(Namespace* _Nonnull self, const char* _Nonnull name, size_t hashCode)
{
    Symbol* sym = self->hashtable[hashCode % self->hashtableCapacity];

    while (sym) {
        if (!strcmp(sym->name, name)) {
            return sym;
        }

        sym = sym->next;
    }

    return NULL;
}

static Symbol* _Nullable Namespace_GetSymbol(Namespace* _Nonnull self, const char* _Nonnull name)
{
    return _Namespace_GetSymbol(self, name, hash_cstring(name));
}

static errno_t Namespace_IterateSymbols(Namespace* _Nonnull self, NameTableIterator _Nonnull cb, void* _Nullable context, bool* _Nonnull pOutDone)
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

static errno_t Namespace_AddSymbol(Namespace* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    decl_try_err();
    Symbol* sym;
    const size_t hashCode = hash_cstring(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Namespace_GetSymbol(self, name, hashCode)) {
        throw(EREDEFINED);
    }

    try(Symbol_Create(name, cb, &sym));
    sym->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = sym;

catch:
    return err;
}



////////////////////////////////////////////////////////////////////////////////
// NameTable
////////////////////////////////////////////////////////////////////////////////

errno_t NameTable_Create(NameTable* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    NameTable* self;

    try_null(self, calloc(1, sizeof(NameTable)), errno);
    try(NameTable_PushNamespace(self));
    
    *pOutSelf = self;
    return EOK;

catch:
    NameTable_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void NameTable_Destroy(NameTable* _Nullable self)
{
    if (self) {
        Namespace* ns = self->currentNamespace;

        while (ns) {
            Namespace* next_ns = ns->parent;

            Namespace_Destroy(ns);
            ns = next_ns;
        }
        self->currentNamespace = NULL;

        free(self);
    }
}

errno_t NameTable_PushNamespace(NameTable* _Nonnull self)
{
    decl_try_err();
    Namespace* ns;

    try(Namespace_Create(self->currentNamespace, &ns));
    self->currentNamespace = ns;

catch:
    return err;
}

errno_t NameTable_PopNamespace(NameTable* _Nonnull self)
{
    if (self->currentNamespace->parent == NULL) {
        return EUNDERFLOW;
    }

    Namespace* ns = self->currentNamespace;
    self->currentNamespace = ns->parent;
    ns->parent = NULL;
    Namespace_Destroy(ns);

    return EOK;
}

static Symbol* _Nullable _NameTable_GetSymbol(NameTable* _Nonnull self, const char* _Nonnull name, Namespace* _Nonnull * _Nullable pOutNamespace)
{
    const size_t hashCode = hash_cstring(name);
    Namespace* ns = self->currentNamespace;

    while (ns) {
        Symbol* sym = _Namespace_GetSymbol(ns, name, hashCode);

        if (sym) {
            if (pOutNamespace) *pOutNamespace = ns;
            return sym;
        }

        ns = ns->parent;
    }

    if (pOutNamespace) *pOutNamespace = NULL;
    return NULL;
}

Symbol* _Nullable NameTable_GetSymbol(NameTable* _Nonnull self, const char* _Nonnull name)
{
    return _NameTable_GetSymbol(self, name, NULL);
}

errno_t NameTable_IterateSymbols(NameTable* _Nonnull self, NameTableIterator _Nonnull cb, void* _Nullable context)
{
    decl_try_err();
    bool done = false;
    Namespace* ns = self->currentNamespace;

    while (ns) {
        err = Namespace_IterateSymbols(ns, cb, context, &done);

        if (err != EOK || done) {
            break;
        }

        ns = ns->parent;
    }

    return err;
}

errno_t NameTable_AddSymbol(NameTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    return Namespace_AddSymbol(self->currentNamespace, name, cb);
}
