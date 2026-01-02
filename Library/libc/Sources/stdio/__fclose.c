//
//  __fclose.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"


static void __deregister_open_file(FILE* _Nonnull s)
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


// Shuts down the given stream but does not free the 's' memory block. 
int __fclose(FILE * _Nonnull s)
{
    const int r1 = __fflush(s);
    const int r2 = (s->cb.close) ? s->cb.close((void*)s->context) : 0;

    __setvbuf(s, NULL, _IONBF, 0);
    __deregister_open_file(s);
    
    return (r1 == 0 && r2 == 0) ? 0 : EOF;
}
