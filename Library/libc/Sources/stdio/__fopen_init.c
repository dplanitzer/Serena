//
//  __fopen_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"
#include <stdlib.h>
#include <string.h>


int __fopen_init(FILE* _Nonnull _Restrict self, void* _Nullable context, const FILE_Callbacks* _Nonnull _Restrict callbacks, __FILE_Mode sm)
{
    int ok = 1;

    if (callbacks == NULL) {
        ok = 0;
    }

    if ((sm & __kStreamMode_Read) != 0 && callbacks->read == NULL) {
        ok = 0;
    }
    if ((sm & __kStreamMode_Write) != 0 && callbacks->write == NULL) {
        ok = 0;
    }

    if (!ok) {
        errno = EINVAL;
        return EOF;
    }

    if ((sm & __kStreamMode_Reinit) == 0) {
        // Init

        memset(self, 0, sizeof(FILE));
        if (mtx_init(&self->lock) != 0) {
            return EOF;
        }

        self->cb = *callbacks;
        self->context = context;
        self->mbstate = (mbstate_t){0};
        self->flags.mode = sm;
        self->flags.bufferMode = _IONBF;
        self->flags.direction = __kStreamDirection_Unknown;
        self->flags.orientation = __kStreamOrientation_Unknown;
        self->flags.shouldFreeOnClose = ((sm & __kStreamMode_FreeOnClose) != 0) ? 1 : 0;
    }
    else {
        // Reinit

        self->cb = *callbacks;
        self->context = context;
        self->mbstate = (mbstate_t){0};
        self->flags.mode = sm;
        self->flags.direction = __kStreamDirection_Unknown;
        self->flags.orientation = __kStreamOrientation_Unknown;
    }

    return 0;
}
