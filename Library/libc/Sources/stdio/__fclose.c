//
//  __fclose.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "__stdio.h"


void __deregister_open_file(FILE* _Nonnull s)
{
    __open_files_lock();

    if (__gOpenFiles == s) {
        __gOpenFiles = s->next;
    }

    if (s->next) {
        (s->next)->prev = s->prev;
    }
    if (s->prev) {
        (s->prev)->next = s->next;
    }

    s->prev = NULL;
    s->next = NULL;

    __open_files_unlock();
}


// Flushes any buffered data to the underlying I/O channel and the closes that
// channel. Does not free the buffer (if one was set up), does not free the
// stream mutex and does not reset and neither free the stream memory block. 
int __fclose(FILE * _Nonnull s)
{
    const int r1 = __fflush(s);
    const int r2 = (s->cb.close) ? s->cb.close((void*)s->context) : 0;
    
    return (r1 == 0 && r2 == 0) ? 0 : EOF;
}
