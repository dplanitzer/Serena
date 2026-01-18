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
#include <ext/hash.h>


////////////////////////////////////////////////////////////////////////////////
// Name
////////////////////////////////////////////////////////////////////////////////

static Name* _Nonnull Name_Create(const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    const size_t len = strlen(name);
    Name* self = calloc(1, sizeof(Name) + len);

    memcpy(self->name, name, len + 1);
    self->cb = cb;

    return self;
}

static void Name_Destroy(Name* _Nullable self)
{
    free(self);
}


////////////////////////////////////////////////////////////////////////////////
// Namespace
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_HASHTABLE_CAPACITY  16

static void _Namespace_DestroyHashtable(Namespace* _Nonnull self);


static Namespace* _Nonnull Namespace_Create(Namespace* _Nullable parentNamespace)
{
    Namespace* self = calloc(1, sizeof(Namespace));

    self->hashtable = calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Name*));
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    self->parent = parentNamespace;
    self->level = (parentNamespace) ? parentNamespace->level + 1 : 0;

    return self;
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
        Name* np = self->hashtable[i];

        while (np) {
            Name* next_np = np->next;

            Name_Destroy(np);
            np = next_np;
        }

        self->hashtable[i] = NULL;
    }

    free(self->hashtable);
    self->hashtable = NULL;
    self->hashtableCapacity = 0;
}

static Name* _Nullable _Namespace_GetName(Namespace* _Nonnull self, const char* _Nonnull name, size_t hashCode)
{
    Name* np = self->hashtable[hashCode % self->hashtableCapacity];

    while (np) {
        if (!strcmp(np->name, name)) {
            return np;
        }

        np = np->next;
    }

    return NULL;
}

static Name* _Nullable Namespace_GetName(Namespace* _Nonnull self, const char* _Nonnull name)
{
    return _Namespace_GetName(self, name, hash_string(name));
}

static errno_t Namespace_IterateSymbols(Namespace* _Nonnull self, NameTableIterator _Nonnull cb, void* _Nullable context, bool* _Nonnull pOutDone)
{
    decl_try_err();
    bool done = false;

    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        Name* np = self->hashtable[i];

        while (np) {
            err = cb(context, np, self->level, &done);

            if (err != EOK || done) {
                break;
            }
        
            np = np->next;
        }
    }

    *pOutDone = done;
    return err;
}

static errno_t Namespace_DeclareName(Namespace* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    decl_try_err();
    Name* np;
    const size_t hashCode = hash_string(name);
    const size_t hashIndex = hashCode % self->hashtableCapacity;

    if (_Namespace_GetName(self, name, hashCode)) {
        throw(EREDEFVAR);
    }

    np = Name_Create(name, cb);
    np->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = np;

catch:
    return err;
}



////////////////////////////////////////////////////////////////////////////////
// NameTable
////////////////////////////////////////////////////////////////////////////////

NameTable* _Nonnull NameTable_Create(void)
{
    NameTable* self = calloc(1, sizeof(NameTable));

    NameTable_PushNamespace(self);
    return self;
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

void NameTable_PushNamespace(NameTable* _Nonnull self)
{
    self->currentNamespace = Namespace_Create(self->currentNamespace);
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

static Name* _Nullable _NameTable_GetName(NameTable* _Nonnull self, const char* _Nonnull name, Namespace* _Nonnull * _Nullable pOutNamespace)
{
    const size_t hashCode = hash_string(name);
    Namespace* ns = self->currentNamespace;

    while (ns) {
        Name* np = _Namespace_GetName(ns, name, hashCode);

        if (np) {
            if (pOutNamespace) *pOutNamespace = ns;
            return np;
        }

        ns = ns->parent;
    }

    if (pOutNamespace) *pOutNamespace = NULL;
    return NULL;
}

Name* _Nullable NameTable_GetName(NameTable* _Nonnull self, const char* _Nonnull name)
{
    return _NameTable_GetName(self, name, NULL);
}

errno_t NameTable_Iterate(NameTable* _Nonnull self, NameTableIterator _Nonnull cb, void* _Nullable context)
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

errno_t NameTable_DeclareName(NameTable* _Nonnull self, const char* _Nonnull name, CommandCallback _Nonnull cb)
{
    return Namespace_DeclareName(self->currentNamespace, name, cb);
}
