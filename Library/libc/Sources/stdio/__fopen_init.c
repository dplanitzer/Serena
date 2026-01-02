//
//  __fopen_init.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <string.h>


static void __register_open_file(FILE* _Nonnull s)
{
    __open_files_lock();

    if (__gOpenFiles) {
        s->prev = NULL;
        s->next = __gOpenFiles;
        __gOpenFiles->prev = s;
        __gOpenFiles = s;
    } else {
        __gOpenFiles = s;
        s->prev = NULL;
        s->next = NULL;
    }

    __open_files_unlock();
}


int __fopen_init(FILE* _Nonnull _Restrict self, bool bFreeOnClose, void* _Nullable context, const FILE_Callbacks* _Nonnull _Restrict callbacks, __FILE_Mode sm)
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

    memset(self, 0, sizeof(FILE));
    self->cb = *callbacks;
    self->context = context;
    self->mbstate = (mbstate_t){0};
    self->flags.mode = sm;
    self->flags.bufferMode = _IONBF;
    self->flags.direction = __kStreamDirection_Unknown;
    self->flags.orientation = __kStreamOrientation_Unknown;
    self->flags.shouldFreeOnClose = bFreeOnClose ? 1 : 0;

    __register_open_file(self);

    return 0;
}
