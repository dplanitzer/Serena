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

static errno_t Constant_Create(ValueType type, RawData data, Constant* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    Constant* self;

    try_null(self, calloc(1, sizeof(Constant)), errno);
    try(Value_Init(&self->value, type, data));

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
        if (cp->value.type == kValue_String && cp->value.u.string.length == len && !memcmp(cp->value.u.string.characters, str, len)) {
            *pOutValue = cp->value;
            return EOK;
        }

        cp = cp->next;
    }

    RawData data = {.string.characters = (char*)str, .string.length = len};
    const errno_t err = Constant_Create(kValue_String, data, &cp);
    if (err == EOK) {
        cp->next = self->hashtable[hashIndex];
        self->hashtable[hashIndex] = cp;
        *pOutValue = cp->value;
        return EOK;
    }
    else {
        Value_InitUndefined(pOutValue);
        return err;
    }
}
