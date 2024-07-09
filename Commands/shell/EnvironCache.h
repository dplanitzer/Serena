//
//  EnvironCache.h
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef EnvironCache_h
#define EnvironCache_h

#include "Errors.h"
#include <stdlib.h>

struct SymbolTable;

typedef struct EnvironEntry {
    struct EnvironEntry* _Nullable  next;   // Next entry in hash chain
    char                            kv[1];  // "key=value" string follows here. Note that the 'key' portion (and just that one) acts as the hashtable key (we treat the '=' as the key string terminator)
} EnvironEntry;

typedef struct EnvironCache {
    struct SymbolTable* _Nonnull    symbolTable;
    EnvironEntry**                  hashtable;
    size_t                          hashtableCapacity;
    char**                          envtable;
    size_t                          envtableCount;
    size_t                          envtableCapacity;
    int                             generation;
} EnvironCache;


extern errno_t EnvironCache_Create(struct SymbolTable* _Nonnull symbolTable, EnvironCache* _Nullable * _Nonnull pOutSelf);
extern void EnvironCache_Destroy(EnvironCache* _Nullable self);

// Returns a pointer to the cached environment variable table. The returned table
// may be empty if. NULL is returned if an error occurred while constructing the
// enviornment.
extern char* _Nullable * _Nullable EnvironCache_GetEnvironment(EnvironCache* _Nonnull self);

#endif  /* EnvironCache_h */
