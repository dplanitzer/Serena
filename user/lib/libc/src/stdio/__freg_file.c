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
    deque_add_last(&__gOpenFiles, &s->qe);
    __open_files_unlock();
}
