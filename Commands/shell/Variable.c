//
//  Variable.c
//  sh
//
//  Created by Dietmar Planitzer on 7/5/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Variable.h"
#include "Utilities.h"
#include <string.h>


// Returns the length of the string that represents the value of the variable.
size_t Variable_GetStringValueLength(const Variable* _Nonnull self)
{
    switch (self->type) {
        case kVariableType_String:
            return self->u.string.length;

        default:
            return 0;
    }
}

// Copies up to 'bufSize' - 1 characters of the variable's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
bool Variable_GetStringValue(const Variable* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    bool copiedAll = false;

    if (bufSize < 1) {
        return false;
    }

    switch (self->type) {
        case kVariableType_String: {
            char* src = self->u.string.characters;

            while (bufSize > 1 && *src != '\0') {
                *buf++ = *src++;
                bufSize--;
            }
            *buf = '\0';
            copiedAll = *src == '\0';
            break;
        }

        default:
            *buf = '\0';
            break;
    }

    return copiedAll;
}
