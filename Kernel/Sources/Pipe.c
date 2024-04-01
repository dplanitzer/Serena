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
#include <System/File.h>


enum {
    kPipeState_Open = 0,
    kPipeState_Closed
};

CLASS_IVARS(Pipe, Object,
    Lock                lock;
    ConditionVariable   reader;
    ConditionVariable   writer;
    RingBuffer          buffer;
    int8_t              readSideState;  // current state of the reader side
    int8_t              writeSideState; // current state of the writer side
);



// Creates a pipe with the given buffer size.
errno_t Pipe_Create(size_t bufferSize, PipeRef _Nullable * _Nonnull pOutPipe)
{
    decl_try_err();
    PipeRef self;

    try(Object_Create(Pipe, &self));
    
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->reader);
    ConditionVariable_Init(&self->writer);
    try(RingBuffer_Init(&self->buffer, __max(bufferSize, 1)));
    self->readSideState = kPipeState_Open;
    self->writeSideState = kPipeState_Open;

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

void Pipe_Close(PipeRef _Nonnull self, unsigned int mode)
{
    Lock_Lock(&self->lock);
    if ((mode & kOpen_ReadWrite) == kOpen_Read) {
        self->readSideState = kPipeState_Closed;
    } else {
        self->writeSideState = kPipeState_Closed;
    }

    // Always wake the reader and the writer since the close may be triggered
    // by an unrelated 3rd process.
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

        while (nBytesRead < nBytesToRead && self->readSideState == kPipeState_Open) {
            const int nChunkSize = RingBuffer_GetBytes(&self->buffer, &((char*)pBuffer)[nBytesRead], nBytesToRead - nBytesRead);

            nBytesRead += nChunkSize;
            if (nChunkSize == 0) {
                if (self->writeSideState == kPipeState_Closed) {
                    err = EOK;
                    break;
                }
                
                if (true /*allowBlocking*/) {
                    // Be sure to wake the writer before we go to sleep and drop the lock
                    // so that it can produce and add data for us.
                    ConditionVariable_BroadcastAndUnlock(&self->writer, NULL);
                    
                    // Wait for the writer to make data available
                    if ((err = ConditionVariable_Wait(&self->reader, &self->lock, kTimeInterval_Infinity)) != EOK) {
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
        
        while (nBytesWritten < nBytesToWrite && self->writeSideState == kPipeState_Open) {
            const int nChunkSize = RingBuffer_PutBytes(&self->buffer, &((char*)pBytes)[nBytesWritten], nBytesToWrite - nBytesWritten);
            
            nBytesWritten += nChunkSize;
            if (nChunkSize == 0) {
                if (self->readSideState == kPipeState_Closed) {
                    err = (nBytesWritten == 0) ? EPIPE : EOK;
                    break;
                }

                if (true /*allowBlocking*/) {
                    // Be sure to wake the reader before we go to sleep and drop the lock
                    // so that it can consume data and make space available to us.
                    ConditionVariable_BroadcastAndUnlock(&self->reader, NULL);
                    
                    // Wait for the reader to make space available
                    if (( err = ConditionVariable_Wait(&self->writer, &self->lock, kTimeInterval_Infinity)) != EOK) {
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


CLASS_METHODS(Pipe, Object,
OVERRIDE_METHOD_IMPL(deinit, Pipe, Object)
);
