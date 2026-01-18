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
#include <ext/hash.h>

////////////////////////////////////////////////////////////////////////////////
// Constant
////////////////////////////////////////////////////////////////////////////////

static Constant* _Nonnull Constant_CreateString(const char* _Nonnull str, size_t len)
{
    Constant* self = calloc(1, sizeof(Constant));

    Value_InitString(&self->value, (char*)str, len, 0);
    return self;
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


ConstantsPool* _Nonnull ConstantsPool_Create(void)
{
    ConstantsPool* self = calloc(1, sizeof(ConstantsPool));

    self->hashtable = calloc(INITIAL_HASHTABLE_CAPACITY, sizeof(Constant*));
    self->hashtableCapacity = INITIAL_HASHTABLE_CAPACITY;

    return self;
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

void ConstantsPool_GetStringValue(ConstantsPool* _Nonnull self, const char* _Nonnull str, size_t len, Value* _Nonnull vp)
{
    const size_t hashCode = hash_bytes(str, len);
    const size_t hashIndex = hashCode % self->hashtableCapacity;
    Constant* cp = self->hashtable[hashIndex];

    while (cp) {
        if (cp->value.type == kValue_String && Value_GetLength(&cp->value) == len && !memcmp(Value_GetCharacters(&cp->value), str, len)) {
            Value_InitCopy(vp, &cp->value);
            return;
        }

        cp = cp->next;
    }

    cp = Constant_CreateString(str, len);
    cp->next = self->hashtable[hashIndex];
    self->hashtable[hashIndex] = cp;
    Value_InitCopy(vp, &cp->value);
}
