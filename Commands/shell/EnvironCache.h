//
//  EnvironCache.h
//  sh
//
//  Created by Dietmar Planitzer on 7/7/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef EnvironCache_h
#define EnvironCache_h

#include <stdlib.h>

struct RunStack;

typedef struct EnvironEntry {
    struct EnvironEntry* _Nullable  next;   // Next entry in hash chain
    char                            kv[1];  // "key=value" string follows here. Note that the 'key' portion (and just that one) acts as the hashtable key (we treat the '=' as the key string terminator)
} EnvironEntry;

typedef struct EnvironCache {
    struct RunStack* _Nonnull   runStack;
    EnvironEntry**              hashtable;
    size_t                      hashtableCapacity;
    char**                      envtable;
    size_t                      envtableCount;
    size_t                      envtableCapacity;
    int                         generation;
} EnvironCache;


extern EnvironCache* _Nonnull EnvironCache_Create(struct RunStack* _Nonnull runStack);
extern void EnvironCache_Destroy(EnvironCache* _Nullable self);

// Returns a pointer to the cached environment variable table. The returned table
// may be empty if. NULL is returned if an error occurred while constructing the
// enviornment.
extern char* _Nullable * _Nullable EnvironCache_GetEnvironment(EnvironCache* _Nonnull self);

#endif  /* EnvironCache_h */
