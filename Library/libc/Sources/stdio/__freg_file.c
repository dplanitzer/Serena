//
//  __freg_file.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


void __freg_file(FILE* _Nonnull s)
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
