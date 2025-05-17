//
//  Pipe.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Pipe.h"
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <klib/RingBuffer.h>
#include <kern/timespec.h>
#include <kpi/stat.h>


final_class_ivars(Pipe, Object,
    Lock                lock;
    ConditionVariable   reader;
    ConditionVariable   writer;
    size_t              readerCount;
    size_t              writerCount;
    RingBuffer          buffer;
);



// Creates a pipe with the given buffer size.
errno_t Pipe_Create(size_t bufferSize, PipeRef _Nullable * _Nonnull pOutPipe)
{
    decl_try_err();
    PipeRef self;

    try(Object_Create(class(Pipe), 0, (void**)&self));
    
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->reader);
    ConditionVariable_Init(&self->writer);
    self->readerCount = 0;
    self->writerCount = 0;
    try(RingBuffer_Init(&self->buffer, __max(bufferSize, 1)));

    *pOutPipe = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutPipe = NULL;
    return err;
}

void Pipe_deinit(PipeRef _Nullable self)
{
    RingBuffer_Deinit(&self->buffer);
    ConditionVariable_Deinit(&self->reader);
    ConditionVariable_Deinit(&self->writer);
    Lock_Deinit(&self->lock);
}

// Returns the number of bytes that can be read from the pipe without blocking.
size_t Pipe_GetNonBlockingReadableCount(PipeRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const size_t nbytes = RingBuffer_ReadableCount(&self->buffer);
    Lock_Unlock(&self->lock);
    return nbytes;
}

// Returns the number of bytes can be written without blocking.
size_t Pipe_GetNonBlockingWritableCount(PipeRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const size_t nbytes = RingBuffer_WritableCount(&self->buffer);
    Lock_Unlock(&self->lock);
    return nbytes;
}

// Returns the maximum number of bytes that the pipe is capable at storing.
size_t Pipe_GetCapacity(PipeRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const size_t nbytes = self->buffer.capacity;
    Lock_Unlock(&self->lock);
    return nbytes;
}

void Pipe_Open(PipeRef _Nonnull self, int end)
{
    Lock_Lock(&self->lock);
    switch (end) {
        case kPipeEnd_Read:
            self->readerCount++;
            break;

        case kPipeEnd_Write:
            self->writerCount++;
            break;

        default:
            abort();
    }

    ConditionVariable_BroadcastAndUnlock(&self->reader, NULL);
    ConditionVariable_BroadcastAndUnlock(&self->writer, &self->lock);
}

void Pipe_Close(PipeRef _Nonnull self, int end)
{
    Lock_Lock(&self->lock);
    switch (end) {
        case kPipeEnd_Read:
            if (self->readerCount > 0) {
                self->readerCount--;
            }
            break;

        case kPipeEnd_Write:
            if (self->writerCount > 0) {
                self->writerCount--;
            }
            break;

        default:
            abort();
    }

    ConditionVariable_BroadcastAndUnlock(&self->reader, NULL);
    ConditionVariable_BroadcastAndUnlock(&self->writer, &self->lock);
}

// Reads up to 'nBytes' from the pipe or until all readable data has been returned.
// Which ever comes first. Blocks the caller if it is asking for more data than
// is available in the pipe. Otherwise all available data is read from the pipe
// and the amount of data read is returned.
errno_t Pipe_Read(PipeRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    ssize_t nBytesRead = 0;

    if (nBytesToRead > 0) {
        Lock_Lock(&self->lock);

        while (nBytesRead < nBytesToRead && self->readerCount > 0) {
            const int nChunkSize = RingBuffer_GetBytes(&self->buffer, &((char*)pBuffer)[nBytesRead], nBytesToRead - nBytesRead);

            nBytesRead += nChunkSize;
            if (nChunkSize == 0) {
                if (self->writerCount == 0) {
                    err = EOK;
                    break;
                }
                
                if (true /*allowBlocking*/) {
                    // Be sure to wake the writer before we go to sleep and drop the lock
                    // so that it can produce and add data for us.
                    ConditionVariable_BroadcastAndUnlock(&self->writer, NULL);
                    
                    // Wait for the writer to make data available
                    if ((err = ConditionVariable_Wait(&self->reader, &self->lock, TIMESPEC_INF)) != EOK) {
                        err = (nBytesRead == 0) ? EINTR : EOK;
                        break;
                    }
                } else {
                    err = (nBytesRead == 0) ? EAGAIN : EOK;
                    break;
                }
            }
        }

        Lock_Unlock(&self->lock);
    }

    *nOutBytesRead = nBytesRead;
    return err;
}

errno_t Pipe_Write(PipeRef _Nonnull self, const void* _Nonnull pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    ssize_t nBytesWritten = 0;

    if (nBytesToWrite > 0) {
        Lock_Lock(&self->lock);
        
        while (nBytesWritten < nBytesToWrite && self->writerCount > 0) {
            const int nChunkSize = RingBuffer_PutBytes(&self->buffer, &((char*)pBytes)[nBytesWritten], nBytesToWrite - nBytesWritten);
            
            nBytesWritten += nChunkSize;
            if (nChunkSize == 0) {
                if (self->readerCount == 0) {
                    err = (nBytesWritten == 0) ? EPIPE : EOK;
                    break;
                }

                if (true /*allowBlocking*/) {
                    // Be sure to wake the reader before we go to sleep and drop the lock
                    // so that it can consume data and make space available to us.
                    ConditionVariable_BroadcastAndUnlock(&self->reader, NULL);
                    
                    // Wait for the reader to make space available
                    if (( err = ConditionVariable_Wait(&self->writer, &self->lock, TIMESPEC_INF)) != EOK) {
                        err = (nBytesWritten == 0) ? EINTR : EOK;
                        break;
                    }
                } else {
                    err = (nBytesWritten == 0) ? EAGAIN : EOK;
                    break;
                }
            }
        }

        Lock_Unlock(&self->lock);
    }

    *nOutBytesWritten = nBytesWritten;    
    return err;
}


class_func_defs(Pipe, Object,
override_func_def(deinit, Pipe, Object)
);
