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


// Returns the length of the string that represents the value of the Value.
size_t Value_GetStringValueLength(const Value* _Nonnull self)
{
    switch (self->type) {
        case kValueType_String:
            return self->u.string.length;

        default:
            return 0;
    }
}

// Copies up to 'bufSize' - 1 characters of the Value's value converted to a
// string to the given buffer. Returns true if the whole value was copied and
// false otherwise.
bool Value_GetStringValue(const Value* _Nonnull self, size_t bufSize, char* _Nonnull buf)
{
    bool copiedAll = false;

    if (bufSize < 1) {
        return false;
    }

    switch (self->type) {
        case kValueType_String: {
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
