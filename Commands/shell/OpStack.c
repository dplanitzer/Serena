//
//  OpStack.c
//  sh
//
//  Created by Dietmar Planitzer on 7/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "OpStack.h"
#include <string.h>

#define INITIAL_STACK_SIZE  16

errno_t OpStack_Create(OpStack* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    OpStack* self;

    try_null(self, calloc(1, sizeof(OpStack)), errno);
    try_null(self->values, malloc(sizeof(Value) * INITIAL_STACK_SIZE), errno);
    self->capacity = INITIAL_STACK_SIZE;
    self->count = 0;
    
    *pOutSelf = self;
    return EOK;

catch:
    OpStack_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void OpStack_Destroy(OpStack* _Nullable self)
{
    if (self) {
        OpStack_PopAll(self);

        free(self->values);
        self->values = NULL;

        free(self);
    }
}

static Value* _Nullable _OpStack_Push(OpStack* _Nonnull self)
{
    if (self->count == self->capacity) {
        const size_t newCapacity = self->capacity * 2;
        Value* newValues = realloc(self->values, sizeof(Value) * newCapacity);

        if (newValues == NULL) {
            return NULL;
        }

        self->values = newValues;
        self->capacity = newCapacity;
    }

    return &self->values[self->count++];
}

errno_t OpStack_Push(OpStack* _Nonnull self, const Value* _Nonnull value)
{
    Value* vp = _OpStack_Push(self);

    if (vp) {
        Value_InitCopy(vp, value);
        return EOK;
    } else {
        return ENOMEM;
    }
}

errno_t OpStack_PushVoid(OpStack* _Nonnull self)
{
    Value* vp = _OpStack_Push(self);

    if (vp) {
        Value_InitVoid(vp);
        return EOK;
    } else {
        return ENOMEM;
    }
}

errno_t OpStack_PushInteger(OpStack* _Nonnull self, int32_t i32)
{
    Value* vp = _OpStack_Push(self);

    if (vp) {
        Value_InitInteger(vp, i32);
        return EOK;
    } else {
        return ENOMEM;
    }
}

errno_t OpStack_PushCString(OpStack* _Nonnull self, const char* str)
{
    Value* vp = _OpStack_Push(self);

    if (vp) {
        if (*str != '\0') {
            return Value_InitCString(vp, str, 0);
        }
        else {
            Value_InitEmptyString(vp);
            return EOK;
        }
    } else {
        return ENOMEM;
    }
}

void OpStack_PopAll(OpStack* _Nonnull self)
{
    for (size_t i = 0; i < self->count; i++) {
        Value_Deinit(&self->values[i]);
    }
    self->count = 0;
}

errno_t OpStack_Pop(OpStack* _Nonnull self)
{
    if (self->count > 0) {
        Value_Deinit(&self->values[--self->count]);
        return EOK;
    }
    else {
        return EUNDERFLOW;
    }
}

errno_t OpStack_PopSome(OpStack* _Nonnull self, size_t count)
{
    if (self->count < count) {
        return EUNDERFLOW;
    }

    while (count-- > 0) {
        Value_Deinit(&self->values[--self->count]);
    }
    return EOK;
}

Value* _Nullable OpStack_GetTos(OpStack* _Nonnull self)
{
    return (self->count > 0) ? &self->values[self->count - 1] : NULL;
}

Value* _Nullable OpStack_GetNth(OpStack* _Nonnull self, size_t idx)
{
    return (self->count > idx) ? &self->values[self->count - (1 + idx)] : NULL;
} 
