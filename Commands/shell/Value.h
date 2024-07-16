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
#include <stdlib.h>

typedef enum ValueType {
    kValueType_String = 0,
} ValueType;


typedef struct StringValue {
    char* _Nonnull  characters;
    size_t          length;
} StringValue;


typedef struct Value {
    int8_t  type;
    int8_t  reserved[3];
    union {
        StringValue string;
    }       u;
} Value;


// Returns the length of the string that represents the value of the Value.
extern size_t Value_GetStringValueLength(const Value* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
extern bool Value_GetStringValue(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf);

#endif  /* Value_h */
