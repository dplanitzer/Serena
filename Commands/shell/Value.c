//
//  Value.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Value.h"
#include "Utilities.h"
#include <string.h>
#include <System/abi/_dmdef.h>
#include <System/_math.h>

#define TUPLE_2(__1, __2) ((__2) << 8) | (__1)
#define TUPLE_3(__1, __2, __3) ((__3) << 16) | ((__2) << 8) | (__1)


errno_t StringValue_Init(Value* _Nonnull self, const char* _Nonnull str, size_t len)
{
    RawData d = {.string.characters = (char*)str, .string.length = len};
    return Value_Init(self, kValue_String, d);
}

errno_t Value_Init(Value* _Nonnull self, ValueType type, RawData data)
{
    decl_try_err();

    self->type = type;
    switch (type) {
        case kValue_Bool:
            self->u.b = data.b;
            break;

        case kValue_Integer:
            self->u.i32 = data.i32;
            break;

        case kValue_String:
            try_null(self->u.string.characters, strdup(data.string.characters), errno);
            self->u.string.length = data.string.length;
            break;

        default:
            break;
    }

catch:
    return err;
}

void Value_Deinit(Value* _Nonnull self)
{
    switch (self->type) {
        case kValue_String:
            free(self->u.string.characters);
            self->u.string.characters = NULL;
            self->u.string.length = 0;
            break;

        default:
            break;
    }

    self->type = kValue_Undefined;
}

errno_t Value_UnaryOp(Value* _Nonnull self, UnaryOperation op)
{
    switch (TUPLE_2(self->type, op)) {
        // Negation
        case TUPLE_2(kValue_Integer, kUnaryOp_Negative):
            self->u.i32 = -self->u.i32;
            return EOK;


        // Not
        case TUPLE_2(kValue_Bool, kUnaryOp_Not):
            self->u.b = !self->u.b;
            return EOK;


        // Others
        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_BinaryOp(Value* _Nonnull lhs_r, const Value* _Nonnull rhs, BinaryOperation op)
{
    switch (TUPLE_3(lhs_r->type, rhs->type, op)) {
        // Equals
        case TUPLE_3(kValue_Bool, kValue_Bool, kBinaryOp_Equals):
            lhs_r->u.b = (lhs_r->u.b == rhs->u.b) ? true : false;
            return EOK;

        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Equals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 == rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Equals): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;
            const size_t lhs_len = lhs_r->u.string.length;
            const size_t rhs_len = rhs->u.string.length;

            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_len == rhs_len && !memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;
            return EOK;
        }


        // NotEquals
        case TUPLE_3(kValue_Bool, kValue_Bool, kBinaryOp_NotEquals):
            lhs_r->u.b = (lhs_r->u.b != rhs->u.b) ? true : false;
            return EOK;

        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_NotEquals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 != rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_NotEquals): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;
            const size_t lhs_len = lhs_r->u.string.length;
            const size_t rhs_len = rhs->u.string.length;

            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_len != rhs_len || memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;
            return EOK;
        }


        // LessEquals
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_LessEquals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 <= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_LessEquals): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (strcmp(lhs_chars, rhs_chars) <= 0) ? true : false;
            return EOK;
        }


        // GreaterEquals
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_GreaterEquals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 >= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_GreaterEquals): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (strcmp(lhs_chars, rhs_chars) >= 0) ? true : false;
            return EOK;
        }


        // Less
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Less):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 < rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Less): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (strcmp(lhs_chars, rhs_chars) < 0) ? true : false;
            return EOK;
        }


        // Greater
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Greater):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 > rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Greater): {
            const char* lhs_chars = lhs_r->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (strcmp(lhs_chars, rhs_chars) > 0) ? true : false;
            return EOK;
        }


        // Addition
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Addition):
            lhs_r->u.i32 = lhs_r->u.i32 + rhs->u.i32;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Addition): {
            const size_t lhs_len = lhs_r->u.string.length;
            const size_t rhs_len = rhs->u.string.length;
            const size_t len = lhs_len + rhs_len;
            char* str = malloc(len + 1);
            if (str == NULL) {
                return ENOMEM;
            }
            memcpy(str, lhs_r->u.string.characters, lhs_len);
            memcpy(&str[lhs_len], rhs->u.string.characters, rhs_len);
            str[len] = '\0';

            lhs_r->u.string.characters = str;
            lhs_r->u.string.length = len;
            return EOK;
        }


        // Subtraction
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Subtraction):
            lhs_r->u.i32 = lhs_r->u.i32 - rhs->u.i32;
            return EOK;


        // Multiplication
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Multiplication):
            lhs_r->u.i32 = lhs_r->u.i32 * rhs->u.i32;
            return EOK;


        // Division
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Division):
            if (rhs->u.i32 == 0) {
                return EDIVBYZERO;
            }

            lhs_r->u.i32 = lhs_r->u.i32 / rhs->u.i32;
            return EOK;


        // Others
        default:
            return ETYPEMISMATCH;
    }
}

// Converts the provided value to its string representation. Does nothing if the
// value is already a string.
errno_t Value_ToString(Value* _Nonnull self)
{
    return ValueArray_ToString(self, 1);
}

// Converts the first value in the provided value array to a string that
// represents the string value of all values in the provided array.
errno_t ValueArray_ToString(Value _Nonnull values[], size_t nValues)
{
    Value* self = &values[0];
    size_t nchars = 0;

    if (nValues == 1 && self->type == kValue_String) {
        return EOK;
    }

    for (size_t i = 0; i < nValues; i++) {
        nchars += Value_GetMaxStringLength(&values[i]);
    }

    char* str = malloc(nchars + 1);
    if (str == NULL) {
        return ENOMEM;
    }

    nchars = 0;
    for (size_t i = 0; i < nValues; i++) {
        nchars += Value_GetString(&values[i], __SIZE_MAX, &str[nchars]);
    }
    str[nchars] = '\0';

    self->type = kValue_String;
    self->u.string.characters = str;
    self->u.string.length = nchars;
    return EOK;
}

// Returns the max length of the string that represents the value of the Value.
// Note that the actual string returned by Value_GetString() may be shorter,
// although it will never be longer than the value returned by this function.
size_t Value_GetMaxStringLength(const Value* _Nonnull self)
{
    switch (self->type) {
        case kValue_Bool:       return 5;   // 'true' or 'false'
        case kValue_Integer:    return __INT_MAX_BASE_10_DIGITS;    // assumes that we always generate a decimal digit strings
        case kValue_String:     return self->u.string.length;
        default:                return 0;
    }
}

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns the number of characters written to 'buf'
// (excluding the nul-terminator).
size_t Value_GetString(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    const char* src;
    size_t nchars = 0;

    if (bufSize < 1) {
        return 0;
    }

    switch (self->type) {
        case kValue_Bool: {
            const char* src = (self->u.b) ? "true" : "false";
            const size_t src_len = (self->u.b) ? 4 : 5;

            nchars = __min(src_len, bufSize - 1);
            break;
        }

        case kValue_Integer: {
            char d[__INT_MAX_BASE_10_DIGITS];

            itoa(self->u.i32, d, 10);
            const size_t len = strlen(d);
            src = d;
            nchars = __min(len, bufSize - 1);
            break;
        }

        case kValue_String:
            src = self->u.string.characters;
            nchars = __min(self->u.string.length, bufSize - 1);
            break;

        default:
            src = "";
            nchars = 0;
            break;
    }

    memcpy(buf, src, nchars);
    buf[nchars] = '\0';
    return nchars;
}

errno_t Value_Write(const Value* _Nonnull self, FILE* _Nonnull stream)
{
    char buf[__INT_MAX_BASE_10_DIGITS];

    switch (self->type) {
        case kValue_Bool: 
            fputs((self->u.b) ? "true" : "false", stream);
            break;

        case kValue_Integer:
            itoa(self->u.i32, buf, 10);
            fputs(buf, stream);
            break;

        case kValue_String:
            fputs(self->u.string.characters, stream);
            break;

        default:
            break;
    }
    return (ferror(stream) ? ferror(stream) : EOK);
}
