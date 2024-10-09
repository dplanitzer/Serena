//
//  Stream.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <string.h>
#include <System/System.h>


static FILE*   gOpenFiles;


// Parses the given mode string into a stream-mode value. Supported modes:
//
// Mode                         Action                          File exists         File does not exist
// -----------------------------------------------------------------------------------------------------
// "r"      read                open for reading                read from start     error
// "w"      write               create & open for writing       truncate file       create
// "a"      append              append to file                  write to end        create
// "r+" 	read extended       open for read/write             read from start     error
// "w+" 	write extended      create & open for read/write    truncate file       create
// "a+" 	append extended     open for read/write             write to end        create
//
// "x" may be used with "w" and "w+". It enables exclusive mode which means that open() will return with
// an error if the file already exists.
errno_t __fopen_parse_mode(const char* _Nonnull mode, __FILE_Mode* _Nonnull pOutMode)
{
    decl_try_err();
    __FILE_Mode sm = 0;

    while (*mode != '\0') {
        if (*mode == 'r') {
            sm |= __kStreamMode_Read;

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Write;
                mode++;
            }
        }
        else if (*mode == 'w') {
            sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Truncate);

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Read;
                mode++;
            }
            if (*(mode + 1) == 'x') {
                sm |= __kStreamMode_Exclusive;
                mode++;
            }
        }
        else if (*mode == 'a') {
            sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Append);

            if (*(mode + 1) == '+') {
                sm |= __kStreamMode_Read;
                mode++;
            }
        }

        mode++;
    }

    if ((sm & (__kStreamMode_Read|__kStreamMode_Write)) == 0) {
        err = EINVAL;
    }
    if (((sm & __kStreamMode_Exclusive) == __kStreamMode_Exclusive) && ((sm & __kStreamMode_Write) == 0)) {
        err = EINVAL;
    }
    if ((sm & (__kStreamMode_Append|__kStreamMode_Truncate)) == (__kStreamMode_Append|__kStreamMode_Truncate)) {
        err = EINVAL;
    }

    *pOutMode = (err == EOK) ? sm : 0;
    return err;
}

errno_t __fopen_init(FILE* _Nonnull self, bool bFreeOnClose, void* context, const FILE_Callbacks* callbacks, __FILE_Mode sm)
{
    if (callbacks == NULL) {
        return EINVAL;
    }

    if ((sm & __kStreamMode_Read) != 0 && callbacks->read == NULL) {
        return EINVAL;
    }
    if ((sm & __kStreamMode_Write) != 0 && callbacks->write == NULL) {
        return EINVAL;
    }

    memset(self, 0, sizeof(FILE));
    self->cb = *callbacks;
    self->context = context;
    self->flags.mode = sm;
    self->flags.mostRecentDirection = __kStreamDirection_None;
    self->flags.shouldFreeOnClose = bFreeOnClose ? 1 : 0;

    if (gOpenFiles) {
        self->next = gOpenFiles;
        gOpenFiles->prev = self;
        gOpenFiles = self;
    } else {
        gOpenFiles = self;
    }

    return EOK;
}

// Shuts down the given stream but does not free the 's' memory block. 
int __fclose(FILE * _Nonnull s)
{
    int r = fflush(s);
        
    const errno_t err = (s->cb.close) ? s->cb.close((void*)s->context) : 0;
    if (r == 0 && err != 0) {
        errno = err;
        r = EOF;
    }

    if (gOpenFiles == s) {
        (s->next)->prev = NULL;
        gOpenFiles = s->next;
    } else {
        (s->next)->prev = s->prev;
        (s->prev)->next = s->next;
    }
    s->prev = NULL;
    s->next = NULL;
    
    return r;
}

int fflush(FILE *s)
{
    int r = 0;

    if (s) {
        // XXX
    }
    else {
        FILE* pCurFile = gOpenFiles;
        
        while (pCurFile) {
            if (pCurFile->flags.mostRecentDirection == __kStreamDirection_Write) {
                const int rx = fflush(pCurFile);

                if (r == 0) {
                    r = rx;
                }
            }
            pCurFile = pCurFile->next;
        }
    }
    return r;
}
