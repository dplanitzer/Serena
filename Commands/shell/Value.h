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


extern errno_t Value_Init(Value* _Nonnull self, ValueType type, RawData data);
extern void Value_Deinit(Value* _Nonnull self);

extern errno_t Value_Negate(const Value* _Nonnull self, Value* _Nonnull r);
extern errno_t Value_Mult(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_Div(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_Add(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_Sub(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);

extern errno_t Value_Equals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_NotEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_Less(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_Greater(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_LessEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);
extern errno_t Value_GreaterEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r);

// Returns the max length of the string that represents the value of the Value.
// Note that the actual string returned by Value_GetString() may be shorter,
// although it will never be longer than the value returned by this function.
extern size_t Value_GetMaxStringLength(const Value* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns the number of characters written to 'buf'
// (excluding the nul-terminator).
extern size_t Value_GetString(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf);

#endif  /* Value_h */
