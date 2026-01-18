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

OpStack* _Nonnull OpStack_Create()
{
    OpStack* self = calloc(1, sizeof(OpStack));

    self->values = malloc(sizeof(Value) * INITIAL_STACK_SIZE);
    self->capacity = INITIAL_STACK_SIZE;
    self->count = 0;
    
    return self;
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

static Value* _Nonnull _OpStack_Push(OpStack* _Nonnull self)
{
    if (self->count == self->capacity) {
        const size_t newCapacity = self->capacity * 2;
        Value* newValues = realloc(self->values, sizeof(Value) * newCapacity);

        self->values = newValues;
        self->capacity = newCapacity;
    }

    return &self->values[self->count++];
}

void OpStack_Push(OpStack* _Nonnull self, const Value* _Nonnull value)
{
    Value* vp = _OpStack_Push(self);
    Value_InitCopy(vp, value);
}

void OpStack_PushVoid(OpStack* _Nonnull self)
{
    Value* vp = _OpStack_Push(self);
    Value_InitVoid(vp);
}

void OpStack_PushBool(OpStack* _Nonnull self, bool flag)
{
    Value* vp = _OpStack_Push(self);
    Value_InitBool(vp, flag);
}

void OpStack_PushInteger(OpStack* _Nonnull self, int32_t i32)
{
    Value* vp = _OpStack_Push(self);
    Value_InitInteger(vp, i32);
}

void OpStack_PushCString(OpStack* _Nonnull self, const char* str)
{
    Value* vp = _OpStack_Push(self);

    if (*str != '\0') {
        Value_InitCString(vp, str, 0);
    }
    else {
        Value_InitEmptyString(vp);
    }
}

void OpStack_PushString(OpStack* _Nonnull self, const char* str, size_t len)
{
    Value* vp = _OpStack_Push(self);

    if (len > 0) {
        Value_InitString(vp, str, len, 0);
    }
    else {
        Value_InitEmptyString(vp);
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

void OpStack_Print(OpStack* _Nonnull self)
{
    printf("op-stack(%d) {\n", (int)self->count);
    for (int i = self->count - 1; i >= 0; i--) {
        fputs("  ", stdout);
        Value_Write(&self->values[i], stdout);
        putchar('\n');
    }
    puts("}");
}