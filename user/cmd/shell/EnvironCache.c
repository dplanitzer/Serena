//
//  EnvironCache.c
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "EnvironCache.h"
#include "RunStack.h"
#include "Utilities.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ext/hash.h>


#define INITIAL_HASHTABLE_CAPACITY  16
#define INITIAL_ENVTABLE_CAPACITY  16

static void _EnvironCache_ClearCache(EnvironCache* _Nonnull self);


EnvironCache* _Nonnull EnvironCache_Create(struct RunStack* _Nonnull runStack)
{
    EnvironCache* self = calloc(1, sizeof(EnvironCache));

    self->hashtable = calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(EnvironEntry*));
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    self->envtable = calloc(INITIAL_ENVTABLE_CAPACITY, sizeof(char*));
    self->envtableCapacity = INITIAL_ENVTABLE_CAPACITY;
    self->runStack = runStack;
    self->generation = -1;

    return self;
}

void EnvironCache_Destroy(EnvironCache* _Nullable self)
{
    if (self) {
        _EnvironCache_ClearCache(self);

        free(self->hashtable);
        self->hashtable = NULL;
        self->hashtableCapacity = 0;

        free(self->envtable);
        self->envtable = NULL;
        self->envtableCapacity = 0;

        free(self);
    }
}

// Frees the key-value pairs stored in the cache but does not free the hash and
// environment tables.
static void _EnvironCache_ClearCache(EnvironCache* _Nonnull self)
{
    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        EnvironEntry* entry = self->hashtable[i];

        while (entry) {
            EnvironEntry* next_entry = entry->next;

            free(entry);
            entry = next_entry;
        }

        self->hashtable[i] = NULL;
    }

    self->envtable[0] = NULL;
    self->envtableCount = 0;
}

// Assumes that 'lhs' is '\0' terminated and that 'rhs' is '=' terminated.
static int ec_strcmp(const char *lhs, const char *rhs)
{
    while (*lhs != '\0' && *rhs != '=' && *lhs == *rhs) {
        lhs++;
        rhs++;
    }

    if (*lhs == '\0' && *rhs == '=') {
        return 0;
    }
    else {
        return *((unsigned char*)lhs) - *((unsigned char*)rhs);
    }
}

static errno_t _EnvironCache_CollectEnvironmentVariables(EnvironCache* _Nonnull self, const Variable* _Nonnull vp, int scopeLevel, bool* _Nonnull pOutDone)
{
    // We only care about exported variables
    if ((vp->modifiers & kVarModifier_Public) == 0) {
        return EOK;
    }


    // Check whether an entry 'name=value' already exists in our hash table. Shadow
    // definitions won't get added since the definition from the newest scope was
    // added first.
    const size_t hashIndex = hash_string(vp->name) % self->hashtableCapacity;
    EnvironEntry* entry = self->hashtable[hashIndex];

    while (entry) {
        if (!ec_strcmp(vp->name, entry->kv)) {
            return EOK;
        }

        entry = entry->next;
    }


    // This is a new variable. Add it
    const size_t keyLen = strlen(vp->name);
    const size_t valLen = Value_GetMaxStringLength(&vp->value);
    const size_t kvLen = keyLen + 1 + valLen; // '\0' is already reserved in EnvironEntry
    EnvironEntry* newEntry = malloc(sizeof(EnvironEntry) + kvLen);


    // Create the 'key=value' string
    memcpy(&newEntry->kv[0], vp->name, keyLen);
    newEntry->kv[keyLen] = '=';
    Value_GetString(&vp->value, valLen + 1, &newEntry->kv[keyLen + 1]);


    // Add the new entry to the hash chain
    newEntry->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = newEntry;
    self->envtableCount++;

    return EOK;
}

static void _EnvironCache_BuildEnvironTable(EnvironCache* _Nonnull self)
{
    if (self->envtableCount + 1 > self->envtableCapacity) {
        self->envtable = realloc(self->envtable, sizeof(char*) * (self->envtableCount + 1));
        if (self->envtable == NULL) {
            _EnvironCache_ClearCache(self);
            return;
        }
    }

    size_t nev = 0;
    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        EnvironEntry* entry = self->hashtable[i];

        if (entry) {
            while (entry) {
                self->envtable[nev++] = entry->kv;
                entry = entry->next;
            }

            assert(nev <= self->envtableCount);
        }
    }
    self->envtable[nev] = NULL;
}

// Updates the cache if it is out-of-date with respect to the current symbol table
// state.
char* _Nullable * _Nullable EnvironCache_GetEnvironment(EnvironCache* _Nonnull self)
{
    const int stGen = RunStack_GetGenerationOfPublicVariables(self->runStack);
    char** ep = NULL;

    if (self->generation != stGen) {
        _EnvironCache_ClearCache(self);

        if (RunStack_Iterate(self->runStack, (RunStackIterator)_EnvironCache_CollectEnvironmentVariables, self) == EOK) {
            _EnvironCache_BuildEnvironTable(self);
            self->generation = stGen;
        }
    }

    return self->envtable;
}
