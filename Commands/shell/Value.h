//
//  Value.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Value_h
#define Value_h

#include "Errors.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum ValueType {
    kValue_Undefined = 0,
    kValue_Bool,
    kValue_Integer,
    kValue_String,
} ValueType;


typedef struct StringValue {
    char* _Nonnull  characters;
    size_t          length;
} StringValue;


// Internal value data representation
typedef union ValueData {
    StringValue string;
    bool        b;
    int32_t     i32;
} ValueData;


// A typed value
typedef struct Value {
    int8_t      type;
    int8_t      reserved[3];
    ValueData   u;
} Value;


// Data used to initialize a new value
typedef union RawData {
    StringValue string;
    bool        b;
    int32_t     i32;
} RawData;


#define Value_InitUndefined(__self) \
    (__self)->type = kValue_Undefined;

#define Value_InitBool(__self, __b) \
    (__self)->type = kValue_Bool; \
    (__self)->u.b = __b;

#define Value_InitInteger(__self, __i32) \
    (__self)->type = kValue_Integer; \
    (__self)->u.i32 = __i32;

extern errno_t Value_Init(Value* _Nonnull self, ValueType type, RawData data);
extern void Value_Deinit(Value* _Nonnull self);

extern errno_t Value_Not(Value* _Nonnull self);

extern errno_t Value_Negate(Value* _Nonnull self);
extern errno_t Value_Mult(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_Div(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_Add(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_Sub(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);

extern errno_t Value_Equals(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_NotEquals(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_Less(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_Greater(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_LessEquals(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);
extern errno_t Value_GreaterEquals(Value* _Nonnull lhs_r, const Value* _Nonnull rhs);

extern errno_t Value_MakeString(Value* _Nonnull self, const Value _Nonnull values[], size_t nValues);

extern errno_t Value_Write(const Value* _Nonnull self, FILE* _Nonnull stream);

// Returns the max length of the string that represents the value of the Value.
// Note that the actual string returned by Value_GetString() may be shorter,
// although it will never be longer than the value returned by this function.
extern size_t Value_GetMaxStringLength(const Value* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns the number of characters written to 'buf'
// (excluding the nul-terminator).
extern size_t Value_GetString(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf);

#endif  /* Value_h */
