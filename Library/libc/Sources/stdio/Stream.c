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
#include <sys/mutex.h>


static FILE*    __gOpenFiles;
static mutex_t  __gOpenFilesLock;


void __init_open_files_lock(void)
{
    mutex_init(&__gOpenFilesLock);
}

static void __open_files_lock(void)
{
    mutex_lock(&__gOpenFilesLock);
}

static void __open_files_unlock(void)
{
    mutex_unlock(&__gOpenFilesLock);
}

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
//
// Modifier                     Action
// -----------------------------------------------------------------------------
// "b"                          open in binary (untranslated) mode
// "t"                          open in translated mode
//
// Modifiers are optional and follow the mode. "b" is always implied on Serena OS.
errno_t __fopen_parse_mode(const char* _Nonnull mode, __FILE_Mode* _Nonnull pOutMode)
{
    decl_try_err();
    __FILE_Mode sm = 0;

    // Mode
    switch (*mode++) {
        case 'r':   sm |= __kStreamMode_Read; break;
        case 'w':   sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Truncate); break;
        case 'a':   sm |= (__kStreamMode_Write | __kStreamMode_Create | __kStreamMode_Append); break;
        default:    *pOutMode = 0; return EINVAL;
    }


    // Modifier
    if (*mode == '+') {
        sm |= (__kStreamMode_Read | __kStreamMode_Write);
        mode++;
    }


    // Any Order Modifiers
    while (*mode != '\0') {
        switch (*mode) {
            case 'x':   sm |= __kStreamMode_Exclusive; break;
            case 'b':   sm |= __kStreamMode_Binary; break;
            case 't':   sm |= __kStreamMode_Text; break;
            default:    break;
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
    if ((sm & (__kStreamMode_Binary | __kStreamMode_Text)) == (__kStreamMode_Binary | __kStreamMode_Text)) {
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

    __register_open_file(self);

    return EOK;
}

// Shuts down the given stream but does not free the 's' memory block. 
int __fclose(FILE * _Nonnull s)
{
    int r = __fflush(s);
        
    const errno_t err = (s->cb.close) ? s->cb.close((void*)s->context) : 0;
    if (r == 0 && err != 0) {
        errno = err;
        r = EOF;
    }

    __deregister_open_file(s);

    return r;
}

// Flushes the buffered data in stream 's'.
int __fflush(FILE * _Nonnull s)
{
    // XXX implement me
    return 0;
}
