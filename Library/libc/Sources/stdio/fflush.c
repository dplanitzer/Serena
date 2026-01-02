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
    FILE* pCurFile;
    
    __open_files_lock();

    pCurFile = __gOpenFiles;
    while (pCurFile) {
        const int rx = f(pCurFile);

        if (r == 0) {
            r = rx;
        }
        pCurFile = pCurFile->next;
    }

    __open_files_unlock();

    return r;
}


int fflush(FILE * _Nonnull s)
{
    if (s) {
        return __fflush(s);
    }
    else {
        return __iterate_open_files(__fflush);
    }
}
