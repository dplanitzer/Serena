//
//  Variable.h
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Variable_h
#define Variable_h

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum VariableType {
    kVariableType_String = 0,
} VariableType;

enum {
    kVariableFlag_Mutable = 1,
    kVariableFlag_Exported = 2,         // Should be included in a command's environment variables
};


typedef struct StringValue {
    char* _Nonnull  characters;
    size_t          length;
} StringValue;


typedef struct Variable {
    int8_t  type;
    uint8_t flags;
    int8_t  reserved[2];
    union {
        StringValue string;
    }       u;
} Variable;


// Returns the length of the string that represents the value of the variable.
extern size_t Variable_GetStringValueLength(const Variable* _Nonnull self);

// Copies up to 'bufSize' - 1 characters of the variable's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
extern bool Variable_GetStringValue(const Variable* _Nonnull self, size_t bufSize, char* _Nonnull buf);

#endif  /* Variable_h */
