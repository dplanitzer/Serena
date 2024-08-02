//
//  ConstantsPool.c
//  sh
//
//  Created by Dietmar Planitzer on 7/22/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ConstantsPool.h"
#include "Utilities.h"
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Constant
////////////////////////////////////////////////////////////////////////////////

static void Constant_Destroy(Constant* _Nullable self);

static errno_t Constant_CreateString(const char* _Nonnull str, size_t len, Constant* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Constant* self;

    try_null(self, calloc(1, sizeof(Constant)), errno);
    try(Value_InitString(&self->value, (char*)str, len, 0));

    *pOutSelf = self;
    return EOK;

catch:
    Constant_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

static void Constant_Destroy(Constant* _Nullable self)
{
    if (self) {
        Value_Deinit(&self->value);
        free(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// ConstantsPool
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_HASHTABLE_CAPACITY  16

static void _ConstantsPool_DestroyHashtable(ConstantsPool* _Nonnull self);


errno_t ConstantsPool_Create(ConstantsPool* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ConstantsPool* self;

    try_null(self, calloc(1, sizeof(ConstantsPool)), errno);
    try_null(self->hashtable, calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Constant*)), errno);
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;

    *pOutSelf = self;
    return EOK;

catch:
    ConstantsPool_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void ConstantsPool_Destroy(ConstantsPool* _Nullable self)
{
    if (self) {
        _ConstantsPool_DestroyHashtable(self);
        free(self);
    }
}

static void _ConstantsPool_DestroyHashtable(ConstantsPool* _Nonnull self)
{
    for (size_t i = 0; i < self->hashtableCapacity; i++) {
        Constant* cp = self->hashtable[i];

        while (cp) {
            Constant* next_cp = cp->next;

            Constant_Destroy(cp);
            cp = next_cp;
        }

        self->hashtable[i] = NULL;
    }

    free(self->hashtable);
    self->hashtable = NULL;
    self->hashtableCapacity = 0;
}

errno_t ConstantsPool_GetStringValue(ConstantsPool* _Nonnull self, const char* _Nonnull str, size_t len, Value* _Nonnull pOutValue)
{
    const size_t hashCode = hash_string(str, len);
    const size_t hashIndex = hashCode % self->hashtableCapacity;
    Constant* cp = self->hashtable[hashIndex];

    while (cp) {
        if (cp->value.type == kValue_String && Value_GetLength(&cp->value) == len && !memcmp(Value_GetCharacters(&cp->value), str, len)) {
            Value_InitCopy(pOutValue, &cp->value);
            return EOK;
        }

        cp = cp->next;
    }

    const errno_t err = Constant_CreateString(str, len, &cp);
    if (err == EOK) {
        cp->next = self->hashtable[hashIndex];
        self->hashtable[hashIndex] = cp;
        Value_InitCopy(pOutValue, &cp->value);
        return EOK;
    }
    else {
        Value_InitUndefined(pOutValue);
        return err;
    }
}
