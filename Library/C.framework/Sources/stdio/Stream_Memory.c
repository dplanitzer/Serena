//
//  Stream_Memory.c
//  Apollo
//
//  Created by Dietmar Planitzer on 1/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <string.h>
#include <apollo/apollo.h>

// We implement POSIX zero-fill gap semantic in the sense that we maintain at
// most one zero-fill gap at the very end of the memory file. This gap is zero-
// filled on __mem_read(). The __mem_write() function hard-fills the portion of
// the zero-fill gap with real 0 values if the current file position is inside
// a sub-range of the zero-fill gap. So this looks like this:
//
//                                  zero-fill gap
// -------------------------------------------------------
// |mmmmmmmmmmmmmmmmmmmmmmmmmmmm|                        |
// -------------------------------------------------------
//    allocated store           storeCapacity            eofPosition
//
typedef struct __FILE_memory {
    char* _Nullable store;
    size_t          currentCapacity;        // current capacity of the backing store
    size_t          maximumCapacity;        // maximum permissible backing store capacity
    size_t          eofPosition;            // has to be >= storeCapacity
    size_t          currentPosition;        // is kept in the range 0...eofPosition
    bool            freeOnClose;
} __FILE_memory;

typedef struct __FILE_compound {
    FILE            file;
    __FILE_memory   mem;
} __FILE_compound;


static errno_t __mem_read(__FILE_memory* _Nonnull mp, void* pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    const ssize_t nBytesRead = (ssize_t)__min((size_t)nBytesToRead, mp->eofPosition - mp->currentPosition);
    const ssize_t nBytesToCopy = (ssize_t)__min((size_t)nBytesRead, mp->currentCapacity - mp->currentPosition);
    const ssize_t nBytesToZero = nBytesRead - nBytesToCopy;

    // This will take care of EOF in a natural way because nBytesRead will be 0
    // and memcpy/memset won't do anything in this case.
    if (nBytesToCopy > 0) {
        memcpy(pBuffer, &mp->store[mp->currentPosition], nBytesToCopy);
        mp->currentPosition += nBytesToCopy;
    }
    if (nBytesToZero > 0) {
        memset(&mp->store[mp->currentPosition], 0, nBytesToZero);
        mp->currentPosition += nBytesToZero;
    }
    *pOutBytesRead = nBytesRead;

    return 0;
}

static errno_t __mem_write(__FILE_memory* _Nonnull mp, const void* pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    // XXX Generate a disk full error if writing would cause the current position to overflow
    size_t newCurrentPosition = mp->currentPosition + nBytesToWrite;

    if (newCurrentPosition > mp->currentCapacity && mp->currentCapacity < mp->maximumCapacity) {
        // This write requires that we expand the backing store
        const size_t autoGrowCapacity = (mp->currentCapacity > 0) ? mp->currentCapacity * 2 : 512;
        const size_t newCapacity = __min(__max(autoGrowCapacity, newCurrentPosition), mp->maximumCapacity);     // the new final position could end up past what auto-grow would give us
        char* pNewStore = realloc(mp->store, newCapacity);

        // Note that we simply fall through to the copying code below if the allocation
        // failed. This is fine because we don't update the backing store pointer and
        // capacity in this case. We do it this way because we may actually be able to
        // write the first X bytes without expanding the store and it's only the second
        // set of Y bytes that require that we expand the store. We want to write to the
        // file what we can write before giving up. 
        if (pNewStore) {
            // Zero-fill the portion of the zero-fill gap that we might have just realized
            // by expanding the backing store.
            const size_t nBytesToClear = (mp->eofPosition > mp->currentCapacity) ? newCapacity - mp->currentCapacity : 0;
            if (nBytesToClear) {
                memset(&pNewStore[mp->currentCapacity], 0, nBytesToClear);
            }

            mp->store = pNewStore;
            mp->currentCapacity = newCapacity;
        }
    }

    // Clamp the number of bytes to write to the available backing store capacity.
    // The "disk" is full if we can't write a single byte.
    const ssize_t nBytesWritten = (ssize_t)__min((size_t)nBytesToWrite, mp->currentCapacity - mp->currentPosition);

    if (nBytesWritten == 0) {
        *pOutBytesWritten = 0;
        return ENOSPC;
    }

    memcpy(&mp->store[mp->currentPosition], pBytes, nBytesWritten);
    mp->currentPosition += nBytesWritten;
    *pOutBytesWritten = nBytesWritten;

    if (mp->currentPosition > mp->eofPosition) {
        mp->eofPosition = mp->currentPosition;
    }

    return 0;
}

static errno_t __mem_seek(__FILE_memory* _Nonnull mp, long long offset, long long* _Nullable outOldOffset, int whence)
{
    size_t newPosition;

    switch (whence) {
        case SEEK_CUR:
            newPosition = mp->currentPosition + offset;
            break;

        case SEEK_END:
            newPosition = mp->eofPosition + offset;
            break;

        case SEEK_SET:
            newPosition = offset;
            break;

        default:
            abort();
            break;
    }

    // XXX do overflow check

    // Expand EOF out if we were told to seek past the current EOF
    if (newPosition > mp->eofPosition) {
        mp->eofPosition = newPosition;
    }

    mp->currentPosition = newPosition;

    return 0;
}

static errno_t __mem_close(__FILE_memory* _Nonnull mp)
{
    if (mp->freeOnClose) {
        free(mp->store);
    }
    mp->store = NULL;

    return 0;
}

static const FILE_Callbacks __FILE_mem_callbacks = {
    (FILE_Read)__mem_read,
    (FILE_Write)__mem_write,
    (FILE_Seek)__mem_seek,
    (FILE_Close)__mem_close
};



FILE *fopen_memory(FILE_Memory *mem, const char *mode)
{
    decl_try_err();
    __FILE_compound* p = NULL;

    try_null(p, malloc(sizeof(__FILE_compound)), ENOMEM);
    FILE* self = &p->file;
    __FILE_memory* mp = &p->mem;

    try(__fopen_init(self, true, (intptr_t)mp, &__FILE_mem_callbacks, mode));
    mp->store = mem->base;
    mp->currentCapacity = mem->initialCapacity;
    mp->maximumCapacity = mem->maximumCapacity;
    mp->eofPosition = mem->initialEof;
    mp->currentPosition = 0;
    mp->freeOnClose = mem->freeOnClose;

    return self;

catch:
    free(p);
    errno = err;
    return NULL;
}

int filemem(FILE *s, FILE_MemoryQuery *query)
{
    if (query == NULL) {
        errno = EINVAL;
        return EOF;
    }

    if (s->cb.read == (FILE_Read)__mem_read) {
        const __FILE_memory* mp = (__FILE_memory*)s->context;

        query->base = mp->store;
        query->eof = mp->eofPosition;
        query->capacity = mp->currentCapacity;
        return 0;
    }
    else {
        query->base = NULL;
        query->eof = 0;
        query->capacity = 0;
        return EOF;
    }
}
