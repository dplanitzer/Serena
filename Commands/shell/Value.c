//
//  Value.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Value.h"
#include "Utilities.h"
#include <assert.h>
#include <string.h>
#include <_dmdef.h>
#include <_math.h>

#define TUPLE_2(__1, __2) ((__2) << 8) | (__1)
#define TUPLE_3(__1, __2, __3) ((__3) << 16) | ((__2) << 8) | (__1)

// Note that non-trivial values (e.g. strings) use a copy-on-write strategy to
// defer copying the backing store until really needed. The backing store is
// reference counted. If a mutation should be done on a non-trivial value then
// the reference count is checked whether it is > 1 before the mutation is
// allowed. If the count is > 1, a copy of the backing store is made and the
// copy is assigned to the value that we want to mutate. Then the mutation can
// proceed on the value.


////////////////////////////////////////////////////////////////////////////////
// Reference Counted String
////////////////////////////////////////////////////////////////////////////////

static errno_t RCString_CreateWithCapacity(size_t capacity, RCString* _Nullable * _Nonnull pOutString)
{
    RCString* self = malloc(sizeof(RCString) + capacity);

    if (self) {
        self->retainCount = 1;
        self->capacity = capacity;
        self->length = 0;

        *pOutString = self;
        return EOK;
    }
    else {
        *pOutString = NULL;
        return ENOMEM;
    }
}

static errno_t RCString_CreateMutableCopy(RCString* _Nonnull other, RCString* _Nullable * _Nonnull pOutString)
{
    RCString* self = NULL;
    const errno_t err = RCString_CreateWithCapacity(other->length + 1, &self);

    if (err == EOK) {
        memcpy(self->characters, other->characters, other->length + 1);
    }
    *pOutString = self;
    return err;
}

static void RCString_SetCharacters(RCString* _Nonnull self, const char* _Nonnull buf, size_t nbytes)
{
    assert(self->capacity > nbytes);

    memcpy(self->characters, buf, nbytes);
    self->characters[nbytes] = '\0';
    self->length = nbytes;
}

static void RCString_Retain(RCString* _Nonnull self)
{
    self->retainCount++;
}

static void RCString_Release(RCString* _Nonnull self)
{
    self->retainCount--;
    if (self->retainCount == 0) {
        free(self);
    }
}


////////////////////////////////////////////////////////////////////////////////
// Value
////////////////////////////////////////////////////////////////////////////////

errno_t Value_InitCString(Value* _Nonnull self, const char* str, ValueFlags flags)
{
    const size_t len = strlen(str);

    self->type = kValue_String;
    self->flags = flags;

    if ((flags & kValueFlag_NoCopy) == kValueFlag_NoCopy) {
        self->u.noCopyString.characters = str;
        self->u.noCopyString.length = len;
        return EOK;
    }
    else {
        const errno_t err = RCString_CreateWithCapacity(len + 1, &self->u.rcString);

        if (err == EOK) {
            RCString_SetCharacters(self->u.rcString, str, len);
        }
        return err;
    }
}

errno_t Value_InitString(Value* _Nonnull self, const char* buf, size_t nbytes, ValueFlags flags)
{
    self->type = kValue_String;
    self->flags = flags;

    if ((flags & kValueFlag_NoCopy) == kValueFlag_NoCopy) {
        self->u.noCopyString.characters = buf;
        self->u.noCopyString.length = nbytes;
        return EOK;
    }
    else {
        const errno_t err = RCString_CreateWithCapacity(nbytes + 1, &self->u.rcString);

        if (err == EOK) {
            RCString_SetCharacters(self->u.rcString, buf, nbytes);
        }
        return err;
    }
}

void Value_InitCopy(Value* _Nonnull self, const Value* _Nonnull other)
{
    *self = *other;

    switch (self->type) {
        case kValue_String:
            if ((self->flags & kValueFlag_NoCopy) == 0) {
                RCString_Retain(self->u.rcString);
            }
            break;

        default:
            break;
    }
}


void Value_Deinit(Value* _Nonnull self)
{
    switch (self->type) {
        case kValue_String:
            if ((self->flags & kValueFlag_NoCopy) == 0) {
                RCString_Release(self->u.rcString);
                self->u.rcString = NULL;
            }
            break;

        default:
            break;
    }

    self->type = kValue_Never;
}

size_t Value_GetLength(const Value* _Nonnull self)
{
    if (self->type == kValue_String) {
        return ((self->flags & kValueFlag_NoCopy) == 0) ? self->u.rcString->length : self->u.noCopyString.length;
    }
    else {
        return 0;
    }
}

// Returns a pointer to the immutable characters of a string value; a pointer to
// an empty string is returned if the value is not a string value.
const char* _Nonnull Value_GetCharacters(const Value* _Nonnull self)
{
    if (self->type == kValue_String) {
        return ((self->flags & kValueFlag_NoCopy) == 0) ? self->u.rcString->characters : self->u.noCopyString.characters;
    }
    else {
        return "";
    }
}

// Returns a pointer to the mutable characters of a string value; a pointer to
// an empty string is returned if the value is not a string value.
char* _Nullable Value_GetMutableCharacters(Value* _Nonnull self)
{
    if (self->type == kValue_String) {
        if (self->u.rcString->retainCount > 1) {
            const errno_t err = RCString_CreateMutableCopy(self->u.rcString, &self->u.rcString);

            if (err != EOK) {
                return NULL;
            }
        }

        return ((self->flags & kValueFlag_NoCopy) == 0) ? self->u.rcString->characters : self->u.noCopyString.characters;
    }
    else {
        return "";
    }
}

errno_t Value_Appending(Value* _Nonnull self, const Value* _Nonnull other)
{
    assert(self->type == kValue_String && other->type == kValue_String);

    decl_try_err();
    const size_t lLen = Value_GetLength(self);
    const size_t rLen = Value_GetLength(other);

    if (rLen == 0) {
        return EOK;
    }

    if (lLen == 0) {
        Value_Deinit(self);
        Value_InitCopy(self, other);
        return EOK;
    }

    RCString* newString = NULL;
    try(RCString_CreateWithCapacity(lLen + rLen + 1, &newString));

    memcpy(newString->characters, self->u.rcString->characters, lLen);
    memcpy(&newString->characters[lLen], other->u.rcString->characters, rLen);
    newString->characters[lLen + rLen] = '\0';
    newString->length = lLen + rLen;

    Value_Deinit(self);
    self->type = kValue_String;
    self->flags = 0;
    self->u.rcString = newString;

catch:
    return err;
}

static int xStrcmp(const char* _Nonnull lhs, size_t lhs_len, const char* _Nonnull rhs, size_t rhs_len)
{
    const size_t common_len = __min(lhs_len, rhs_len);
    const int r = memcmp(lhs, rhs, common_len);

    if (r != 0) {
        return r;
    }
    else {
        if (lhs_len < rhs_len) {
            return -1;
        }
        else if (lhs_len > rhs_len) {
            return 1;
        }
        else {
            return 0;
        }
    }
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
            return (self->type == kValue_Never) ? ENOVAL : ETYPEMISMATCH;
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
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (lhs_len == rhs_len && !memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
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
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (lhs_len != rhs_len || memcmp(lhs_chars, rhs_chars, lhs_len)) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
            return EOK;
        }


        // LessEquals
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_LessEquals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 <= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_LessEquals): {
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (xStrcmp(lhs_chars, lhs_len, rhs_chars, rhs_len) <= 0) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
            return EOK;
        }


        // GreaterEquals
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_GreaterEquals):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 >= rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_GreaterEquals): {
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (xStrcmp(lhs_chars, lhs_len, rhs_chars, rhs_len) >= 0) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
            return EOK;
        }


        // Less
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Less):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 < rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Less): {
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (xStrcmp(lhs_chars, lhs_len, rhs_chars, rhs_len) < 0) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
            return EOK;
        }


        // Greater
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Greater):
            lhs_r->type = kValue_Bool;
            lhs_r->u.b = (lhs_r->u.i32 > rhs->u.i32) ? true : false;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Greater): {
            const char* lhs_chars = Value_GetCharacters(lhs_r);
            const char* rhs_chars = Value_GetCharacters(rhs);
            const size_t lhs_len = Value_GetLength(lhs_r);
            const size_t rhs_len = Value_GetLength(rhs);
            const bool r = (xStrcmp(lhs_chars, lhs_len, rhs_chars, rhs_len) > 0) ? true : false;

            Value_Deinit(lhs_r);
            Value_InitBool(lhs_r, r);
            return EOK;
        }


        // Addition
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Addition):
            lhs_r->u.i32 = lhs_r->u.i32 + rhs->u.i32;
            return EOK;

        case TUPLE_3(kValue_String, kValue_String, kBinaryOp_Addition):
            return Value_Appending(lhs_r, rhs);


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
            if (rhs->u.i32 != 0) {
                lhs_r->u.i32 = lhs_r->u.i32 / rhs->u.i32;
                return EOK;
            } else {
                return EDIVBYZERO;
            }


        // Modulo
        case TUPLE_3(kValue_Integer, kValue_Integer, kBinaryOp_Modulo):
            if (rhs->u.i32 != 0) {
                lhs_r->u.i32 = lhs_r->u.i32 % rhs->u.i32;
                return EOK;
            } else {
                return EDIVBYZERO;
            }


        // Others
        default:
            if (lhs_r->type == kValue_Never || rhs->type == kValue_Never) {
                return ENOVAL;
            }
            else {
                return ETYPEMISMATCH;
            }
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
    decl_try_err();
    Value* self = &values[0];
    size_t nchars = 0;

    if (nValues == 1 && self->type == kValue_String) {
        return EOK;
    }

    for (size_t i = 0; i < nValues; i++) {
        nchars += Value_GetMaxStringLength(&values[i]);
    }

    RCString* newString = NULL;
    try(RCString_CreateWithCapacity(nchars + 1, &newString));

    nchars = 0;
    for (size_t i = 0; i < nValues; i++) {
        nchars += Value_GetString(&values[i], __SIZE_MAX, &newString->characters[nchars]);
    }
    newString->characters[nchars] = '\0';
    newString->length = nchars;

    Value_Deinit(self);
    self->type = kValue_String;
    self->flags = 0;
    self->u.rcString = newString;

catch:
    return err;
}

// Returns the max length of the string that represents the value of the Value.
// Note that the actual string returned by Value_GetString() may be shorter,
// although it will never be longer than the value returned by this function.
size_t Value_GetMaxStringLength(const Value* _Nonnull self)
{
    switch (self->type) {
        case kValue_Void:       return 4;   // 'void'
        case kValue_Bool:       return 5;   // 'true' or 'false'
        case kValue_Integer:    return __INT_MAX_BASE_10_DIGITS;    // assumes that we always generate a decimal digit strings
        case kValue_String:     return Value_GetLength(self);
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
        case kValue_Void:
            src = "void";
            nchars = 4;
            break;

        case kValue_Bool: {
            const size_t src_len = (self->u.b) ? 4 : 5;

            src = (self->u.b) ? "true" : "false";
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
            src = Value_GetCharacters(self);
            nchars = __min(Value_GetLength(self), bufSize - 1);
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
        case kValue_Void:
            fputs("void", stream);
            break;
            
        case kValue_Bool: 
            fputs((self->u.b) ? "true" : "false", stream);
            break;

        case kValue_Integer:
            itoa(self->u.i32, buf, 10);
            fputs(buf, stream);
            break;

        case kValue_String:
            fputs(Value_GetCharacters(self), stream);
            break;

        default:
            break;
    }
    return (ferror(stream) ? ferror(stream) : EOK);
}
