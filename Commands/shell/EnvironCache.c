//
//  EnvironCache.c
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "EnvironCache.h"
#include "SymbolTable.h"
#include "builtins/cmdlib.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


#define INITIAL_HASHTABLE_CAPACITY  16
#define INITIAL_ENVTABLE_CAPACITY  16

static void _EnvironCache_ClearCache(EnvironCache* _Nonnull self);


errno_t EnvironCache_Create(struct SymbolTable* _Nonnull symbolTable, EnvironCache* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    EnvironCache* self;

    try_null(self, calloc(1, sizeof(EnvironCache)), errno);
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(EnvironEntry*)), errno);
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;
    try_null(self->envtable, calloc(INITIAL_ENVTABLE_CAPACITY, sizeof(char*)), errno);
    self->envtableCapacity = INITIAL_ENVTABLE_CAPACITY;
    self->symbolTable = symbolTable;
    self->generation = -1;

    *pOutSelf = self;
    return EOK;

catch:
    EnvironCache_Destroy(self);
    *pOutSelf = NULL;
    return err;
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

static errno_t _EnvironCache_CollectEnvironmentVariables(EnvironCache* _Nonnull self, const Symbol* _Nonnull symbol, int scopeLevel, bool* _Nonnull pOutDone)
{
    // We only care about exported variables
    if (symbol->type != kSymbolType_Variable || (symbol->type == kSymbolType_Variable && (symbol->u.variable.flags & kVariableFlag_Exported) == 0)) {
        return EOK;
    }


    // Check whether an entry 'name=value' already exists in our hash table. Shadow
    // definitions won't get added since the definition from the newest scope was
    // added first.
    const size_t hashIndex = hash_cstring(symbol->name) % self->hashtableCapacity;
    EnvironEntry* entry = self->hashtable[hashIndex];

    while (entry) {
        if (!ec_strcmp(symbol->name, entry->kv)) {
            return EOK;
        }

        entry = entry->next;
    }


    // This is a new variable. Add it
    const size_t keyLen = strlen(symbol->name);
    const size_t valLen = Variable_GetStringValueLength(&symbol->u.variable);
    const size_t kvLen = keyLen + 1 + valLen; // '\0' is already reserved in EnvironEntry
    EnvironEntry* newEntry = malloc(sizeof(EnvironEntry) + kvLen);
    if (newEntry == NULL) {
        return errno;
    }


    // Create the 'key=value' string
    memcpy(&newEntry->kv[0], symbol->name, keyLen);
    newEntry->kv[keyLen] = '=';
    Variable_GetStringValue(&symbol->u.variable, valLen, &newEntry->kv[keyLen + 1]);


    // Add the new entry to the hash chain
    newEntry->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = newEntry;
    self->envtableCount++;

    return EOK;
}

static errno_t _EnvironCache_BuildEnvironTable(EnvironCache* _Nonnull self)
{
    if (self->envtableCount + 1 > self->envtableCapacity) {
        self->envtable = realloc(self->envtable, sizeof(char*) * (self->envtableCount + 1));
        if (self->envtable == NULL) {
            _EnvironCache_ClearCache(self);
            return errno;
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

    return EOK;
}

// Updates the cache if it is out-of-date with respect to the current symbol table
// state.
char* _Nullable * _Nullable EnvironCache_GetEnvironment(EnvironCache* _Nonnull self)
{
    const int stGen = SymbolTable_GetExportedVariablesGeneration(self->symbolTable);
    char** ep = NULL;

    if (self->generation != stGen) {
        _EnvironCache_ClearCache(self);

        if (SymbolTable_IterateSymbols(self->symbolTable, (SymbolTableIterator)_EnvironCache_CollectEnvironmentVariables, self) == EOK) {
            if (_EnvironCache_BuildEnvironTable(self) == EOK) {
                self->generation = stGen;
            }
        }
    }

    return self->envtable;
}
