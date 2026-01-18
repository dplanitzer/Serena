//
//  Value.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Value_h
#define Value_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ext/errno.h>

typedef enum ValueType {
    kValue_Never = 0,           // Bottom type (uninhabited)
    //kValue_Any,                  Top type (default for variable without explicit type information)
    kValue_Void,
    kValue_Bool,
    kValue_Integer,
    kValue_String,
} ValueType;


typedef enum ValueFlags {
    kValueFlag_NoCopy = 1,      // Set if the value does not own the string backing store (it's provided and owned by someone outside the VM)
} ValueFlags;


typedef struct RCString {
    int32_t retainCount;
    size_t  capacity;       // Includes the trailing nul
    size_t  length;         // Excludes the trailing nul
    char    characters[1];
} RCString;


// Internal value data representation
typedef union ValueData {
    struct NoCopyString {
        const char* _Nonnull    characters;
        size_t                  length;
    }                   noCopyString;
    RCString* _Nonnull  rcString;
    bool                b;
    int32_t             i32;
} ValueData;


// A typed value
typedef struct Value {
    uint8_t     type;
    uint8_t     flags;
    uint8_t     reserved[2];
    ValueData   u;
} Value;


#define Value_InitNever(__self) \
    (__self)->type = kValue_Never; \
    (__self)->flags = 0

#define Value_InitVoid(__self) \
    (__self)->type = kValue_Void; \
    (__self)->flags = 0

#define Value_InitBool(__self, __b) \
    (__self)->type = kValue_Bool; \
    (__self)->flags = 0; \
    (__self)->u.b = (__b)

#define Value_InitInteger(__self, __i32) \
    (__self)->type = kValue_Integer; \
    (__self)->flags = 0; \
    (__self)->u.i32 = (__i32)

#define Value_InitEmptyString(__self) \
    Value_InitCString(__self, "", kValueFlag_NoCopy)

extern void Value_InitCString(Value* _Nonnull self, const char* str, ValueFlags flags);
extern void Value_InitString(Value* _Nonnull self, const char* buf, size_t nbytes, ValueFlags flags);

// Creates an efficient copy of 'other'. The copy is in the sense efficient that
// 'other' and 'self' will share the same backing store if 'other' is a string.
// The backing store is only copied if either value is mutated later on.
extern void Value_InitCopy(Value* _Nonnull self, const Value* _Nonnull other);

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
    kBinaryOp_Division,         // .
    kBinaryOp_Modulo,           // (kArithmetic_Modulo)
} BinaryOperation;

extern errno_t Value_BinaryOp(Value* _Nonnull lhs_r, const Value* _Nonnull rhs, BinaryOperation op);

// Converts the provided value to its string representation. Does nothing if the
// value is already a string.
extern void Value_ToString(Value* _Nonnull self);

// Converts the first value in the provided value array to a string that
// represents the string value of all values in the provided array.
extern void ValueArray_ToString(Value _Nonnull values[], size_t nValues);

// Writes the string representation of the given value to the given I/O stream.
extern void Value_Write(const Value* _Nonnull self, FILE* _Nonnull stream);

// Returns the length of a string value; 0 is returned if the value is not a
// string.
extern size_t Value_GetLength(const Value* _Nonnull self);

// Returns a pointer to the immutable characters of a string value; a pointer to
// an empty string is returned if the value is not a string value.
extern const char* _Nonnull Value_GetCharacters(const Value* _Nonnull self);

// Returns a pointer to the mutable characters of a string value; a NULL pointer
// is returned if this is not possible. Note that this function triggers a copy-
// on-write operation if the string backing store is shared with some other
// value.
extern char* _Nonnull Value_GetMutableCharacters(Value* _Nonnull self);

// Assumes that the receiver and 'other' are strings and concatenates the string
// values and assigns the result to 'self'.
extern void Value_Appending(Value* _Nonnull self, const Value* _Nonnull other);

// Returns the max length of the string that represents the value of the Value.
// Note that the actual string returned by Value_GetString() may be shorter,
// although it will never be longer than the value returned by this function.
extern size_t Value_GetMaxStringLength(const Value* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns the number of characters written to 'buf'
// (excluding the nul-terminator).
extern size_t Value_GetString(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf);

#endif  /* Value_h */
