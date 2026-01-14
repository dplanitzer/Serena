//
//  fflush.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


// Iterates through all registered files and invokes 'f' on each one. Note that
// this function holds the file registration lock while invoking 'f'. 
int __iterate_open_files(__file_func_t _Nonnull f)
{
    int r = 0;
    
    __open_files_lock();
    queue_for_each(&__gOpenFiles, FILE, it,
        const int rx = f(it);

        if (r == 0) {
            r = rx;
        }
    );
    __open_files_unlock();

    return r;
}


static int __fflush_locked(FILE* _Nonnull s)
{
    __flock(s);
    const int r = __fflush(s);
    __funlock(s);

    return r;
}

int fflush(FILE * _Nonnull s)
{
    if (s) {
        return __fflush_locked(s);
    }
    else {
        return __iterate_open_files(__fflush_locked);
    }
}
