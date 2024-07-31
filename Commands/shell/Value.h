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
    kValue_Void,
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


#define UndefinedValue_Init(__self) \
    (__self)->type = kValue_Undefined;

#define VoidValue_Init(__self) \
    (__self)->type = kValue_Void;

#define BoolValue_Init(__self, __b) \
    (__self)->type = kValue_Bool; \
    (__self)->u.b = __b;

#define IntegerValue_Init(__self, __i32) \
    (__self)->type = kValue_Integer; \
    (__self)->u.i32 = __i32;

extern errno_t StringValue_Init(Value* _Nonnull self, const char* _Nonnull str, size_t len);

extern errno_t Value_Init(Value* _Nonnull self, ValueType type, RawData data);
extern errno_t Value_InitCopy(Value* _Nonnull self, const Value* _Nonnull other);
extern void Value_Deinit(Value* _Nonnull self);


typedef enum UnaryOperation {
    kUnaryOp_Negative,          // (kArithmetic_Negative)
    kUnaryOp_Not,               // (kArithmetic_Not)
} UnaryOperation;

extern errno_t Value_UnaryOp(Value* _Nonnull self, UnaryOperation op);


typedef enum BinaryOperation {
    kBinaryOp_Equals,           // (kArithmetic_Equals)
    kBinaryOp_NotEquals,        // .
    kBinaryOp_LessEquals,       // .
    kBinaryOp_GreaterEquals,    // .
    kBinaryOp_Less,             // .
    kBinaryOp_Greater,          // .
    kBinaryOp_Addition,         // .
    kBinaryOp_Subtraction,      // .
    kBinaryOp_Multiplication,   // .
    kBinaryOp_Division,         // (kArithmetic_Division)
} BinaryOperation;

extern errno_t Value_BinaryOp(Value* _Nonnull lhs_r, const Value* _Nonnull rhs, BinaryOperation op);

// Converts the provided value to its string representation. Does nothing if the
// value is already a string.
extern errno_t Value_ToString(Value* _Nonnull self);

// Converts the first value in the provided value array to a string that
// represents the string value of all values in the provided array.
extern errno_t ValueArray_ToString(Value _Nonnull values[], size_t nValues);

// Writes the string representation of the given value to the given I/O stream.
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
