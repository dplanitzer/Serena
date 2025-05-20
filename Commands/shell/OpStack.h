//
//  OpStack.h
//  sh
//
//  Created by Dietmar Planitzer on 7/15/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef OpStack_h
#define OpStack_h

#include "Errors.h"
#include "Value.h"


typedef struct OpStack {
    Value* _Nonnull values;
    size_t          capacity;
    size_t          count;
} OpStack;


extern OpStack* _Nonnull OpStack_Create(void);
extern void OpStack_Destroy(OpStack* _Nullable self);

// Pushes the given value on top of the operand stack.
extern errno_t OpStack_Push(OpStack* _Nonnull self, const Value* _Nonnull value);

extern errno_t OpStack_PushVoid(OpStack* _Nonnull self);
extern errno_t OpStack_PushBool(OpStack* _Nonnull self, bool flag);
extern errno_t OpStack_PushInteger(OpStack* _Nonnull self, int32_t i32);
extern errno_t OpStack_PushCString(OpStack* _Nonnull self, const char* str);
extern errno_t OpStack_PushString(OpStack* _Nonnull self, const char* str, size_t len);

// Pops all values from the stack.
extern void OpStack_PopAll(OpStack* _Nonnull self);

// Pops the top-most entry off the operand stack.
extern errno_t OpStack_Pop(OpStack* _Nonnull self);

// Pops the top-most 'count' entries off the operand stack.
extern errno_t OpStack_PopSome(OpStack* _Nonnull self, size_t count);

// Returns true if the operand stack is empty
#define OpStack_IsEmpty(__self) (((__self)->count == 0) ? true : false)

// Returns a pointer to the top-most entry on the operand stack. NULL is returned
// if the stack is empty.
extern Value* _Nullable OpStack_GetTos(OpStack* _Nonnull self);

// Returns a pointer to the entry at index 'idx' relative to the top of the
// operand stack. NULL is returned on an underflow.
extern Value* _Nullable OpStack_GetNth(OpStack* _Nonnull self, size_t idx);

extern void OpStack_Print(OpStack* _Nonnull self);

#endif  /* OpStack_h */
