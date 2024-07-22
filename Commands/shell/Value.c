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

#define TUPLE(__t) ((__t) << 8) | (__t)
#define TUPLE_2(__lhs, __rhs) ((__lhs) << 8) | (__rhs)


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

errno_t Value_Negate(const Value* _Nonnull self, Value* _Nonnull r)
{
    switch (self->type) {
        case kValue_Bool:
            r->type = kValue_Bool;
            r->u.b = !self->u.b;
            return EOK;

        case kValue_Integer:
            r->type = kValue_Integer;
            r->u.i32 = -self->u.i32;
            return EOK;

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Mult(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Integer):
            r->type = kValue_Integer;
            r->u.i32 = lhs->u.i32 * rhs->u.i32;
            return EOK;

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Div(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Integer):
            if (rhs->u.i32 == 0) {
                return EDIVBYZERO;
            }

            r->type = kValue_Integer;
            r->u.i32 = lhs->u.i32 / rhs->u.i32;
            return EOK;

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Add(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Integer):
            r->type = kValue_Integer;
            r->u.i32 = lhs->u.i32 + rhs->u.i32;
            return EOK;

        case TUPLE(kValue_String): {
            const size_t lhs_len = lhs->u.string.length;
            const size_t rhs_len = rhs->u.string.length;
            const size_t len = lhs_len + rhs_len;
            char* str = malloc(len + 1);
            if (str == NULL) {
                return ENOMEM;
            }
            memcpy(str, lhs->u.string.characters, lhs_len);
            memcpy(&str[lhs_len], rhs->u.string.characters, rhs_len);
            str[len] = '\0';
            r->type = kValue_String;
            r->u.string.characters = str;
            r->u.string.length = len;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Sub(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Integer):
            r->type = kValue_Integer;
            r->u.i32 = lhs->u.i32 - rhs->u.i32;
            return EOK;

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Equals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b == rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 == rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;
            const size_t lhs_len = lhs->u.string.length;
            const size_t rhs_len = rhs->u.string.length;

            r->type = kValue_Bool;
            r->u.b = (lhs_len == rhs_len && !memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_NotEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b != rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 != rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;
            const size_t lhs_len = lhs->u.string.length;
            const size_t rhs_len = rhs->u.string.length;

            r->type = kValue_Bool;
            r->u.b = (lhs_len != rhs_len || memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Less(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b < rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 < rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            r->type = kValue_Bool;
            r->u.b = (strcmp(lhs_chars, rhs_chars) < 0) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_Greater(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b > rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 > rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            r->type = kValue_Bool;
            r->u.b = (strcmp(lhs_chars, rhs_chars) > 0) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_LessEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b <= rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 <= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            r->type = kValue_Bool;
            r->u.b = (strcmp(lhs_chars, rhs_chars) <= 0) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
}

errno_t Value_GreaterEquals(const Value* _Nonnull lhs, const Value* _Nonnull rhs, Value* _Nonnull r)
{
    switch (TUPLE_2(lhs->type, rhs->type)) {
        case TUPLE(kValue_Bool):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.b >= rhs->u.b) ? true : false;
            break;

        case TUPLE(kValue_Integer):
            r->type = kValue_Bool;
            r->u.b = (lhs->u.i32 >= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE(kValue_String): {
            const char* lhs_chars = lhs->u.string.characters;
            const char* rhs_chars = rhs->u.string.characters;

            // lexicographical order
            r->type = kValue_Bool;
            r->u.b = (strcmp(lhs_chars, rhs_chars) >= 0) ? true : false;
            return EOK;
        }

        default:
            return ETYPEMISMATCH;
    }
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

errno_t Value_MakeString(Value* _Nonnull self, const Value _Nonnull values[], size_t nValues)
{
    size_t nchars = 0;

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
