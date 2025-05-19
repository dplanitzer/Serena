//
//  Stream_Memory.c
//  libc
//
//  Created by Dietmar Planitzer on 1/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "Stream.h"
#include <stdlib.h>
#include <string.h>
#include <sys/limits.h>


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


static ssize_t __mem_read(__Memory_FILE_Vars* _Nonnull mp, void* pBuffer, ssize_t nBytesToRead)
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

    return nBytesRead;
}

static ssize_t __mem_write(__Memory_FILE_Vars* _Nonnull mp, const void* pBytes, ssize_t nBytesToWrite)
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
        errno = ENOSPC;
        return 0;
    }

    memcpy(&mp->store[mp->currentPosition], pBytes, nBytesWritten);
    mp->currentPosition += nBytesWritten;

    if (mp->currentPosition > mp->eofPosition) {
        mp->eofPosition = mp->currentPosition;
    }

    return nBytesWritten;
}

static long long __mem_seek(__Memory_FILE_Vars* _Nonnull mp, long long offset, int whence)
{
    long long newOffset;

    switch (whence) {
        case SEEK_CUR:
            newOffset = (long long)mp->currentPosition + offset;
            break;

        case SEEK_END:
            newOffset = (long long)mp->eofPosition + offset;
            break;

        case SEEK_SET:
            newOffset = offset;
            break;

        default:
            errno = EINVAL;
            return EOF;
    }

    if (newOffset < 0 || newOffset > SIZE_MAX) {
        errno = EOVERFLOW;
        return EOF;
    }

    // Expand EOF out if we were told to seek past the current EOF. Note that
    // the next __mem_read() and __mem_write() will take care of the range check
    // and actual backing store expansion.
    const size_t oldPos = mp->currentPosition;
    const size_t newPos = (size_t)newOffset;
    if (newPos > mp->eofPosition) {
        mp->eofPosition = newPos;
    }
    mp->currentPosition = newPos;

    return oldPos;
}

static int __mem_close(__Memory_FILE_Vars* _Nonnull mp)
{
    if (mp->flags.freeOnClose) {
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



int __fopen_memory_init(__Memory_FILE* _Nonnull self, bool bFreeOnClose, FILE_Memory *mem, __FILE_Mode sm)
{
    __Memory_FILE_Vars* mp = &self->v;

    mp->store = mem->base;
    mp->currentCapacity = mem->initialCapacity;
    mp->maximumCapacity = mem->maximumCapacity;

    if ((sm & __kStreamMode_Append) == __kStreamMode_Append) {
        mp->currentPosition = mem->initialEof;
        mp->eofPosition = mem->initialEof;
    }
    else if ((sm & __kStreamMode_Truncate) == __kStreamMode_Truncate) {
        mp->currentPosition = 0;
        mp->eofPosition = 0;
    }
    else {
        mp->currentPosition = 0;
        mp->eofPosition = mem->initialEof;
    }

    mp->flags.freeOnClose = ((mem->options & _IOM_FREE_ON_CLOSE) != 0) ? 1 : 0;

    return __fopen_init((FILE*)self, bFreeOnClose, mp, &__FILE_mem_callbacks, sm);
}

int filemem(FILE *s, FILE_MemoryQuery *query)
{
    if (query == NULL) {
        errno = EINVAL;
        return EOF;
    }

    if (s->cb.read == (FILE_Read)__mem_read) {
        const __Memory_FILE_Vars* mp = (__Memory_FILE_Vars*)s->context;

        query->base = mp->store;
        query->eof = mp->eofPosition;
        query->capacity = mp->currentCapacity;
        return EOK;
    }
    else {
        query->base = NULL;
        query->eof = 0;
        query->capacity = 0;
        return EOF;
    }
}
