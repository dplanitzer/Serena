//
//  Pipe.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Pipe.h"
#include "ConditionVariable.h"
#include "Lock.h"
#include "RingBuffer.h"


enum {
    kPipeState_Open = 0,
    kPipeState_Closed
};

typedef struct _Pipe {
    Lock                lock;
    ConditionVariable   reader;
    ConditionVariable   writer;
    RingBuffer          buffer;
    Int8                readSideState;  // current state of the reader side
    Int8                writeSideState; // current state of the writer side
} Pipe;



// Creates a pipe with the given buffer size.
ErrorCode Pipe_Create(Int bufferSize, PipeRef _Nullable * _Nonnull pOutPipe)
{
    decl_try_err();
    PipeRef pPipe;

    assert(bufferSize > 0);
        
    try(kalloc(sizeof(Pipe), (Byte**) &pPipe));
    
    Lock_Init(&pPipe->lock);
    ConditionVariable_Init(&pPipe->reader);
    ConditionVariable_Init(&pPipe->writer);
    try(RingBuffer_Init(&pPipe->buffer, bufferSize));
    pPipe->readSideState = kPipeState_Open;
    pPipe->writeSideState = kPipeState_Open;

    *pOutPipe = pPipe;
    return EOK;
    
catch:
    Pipe_Destroy(pPipe);
    *pOutPipe = NULL;
    return err;
}

void Pipe_Destroy(PipeRef _Nullable pPipe)
{
    if (pPipe) {
        RingBuffer_Deinit(&pPipe->buffer);
        ConditionVariable_Deinit(&pPipe->reader);
        ConditionVariable_Deinit(&pPipe->writer);
        Lock_Deinit(&pPipe->lock);
        kfree((Byte*)pPipe);
    }
}

// Closes the specified side of the pipe.
void Pipe_Close(PipeRef _Nonnull pPipe, PipeSide side)
{
    Lock_Lock(&pPipe->lock);
    switch (side) {
        case kPipe_Reader:
            pPipe->readSideState = kPipeState_Closed;
            break;
            
        case kPipe_Writer:
            pPipe->writeSideState = kPipeState_Closed;
            break;
    }

    ConditionVariable_BroadcastAndUnlock(&pPipe->reader, &pPipe->lock);
    ConditionVariable_BroadcastAndUnlock(&pPipe->writer, &pPipe->lock);
}

// Returns the number of bytes that can be read from the pipe without blocking if
// 'side' is kPipe_Reader and the number of bytes the can be written without blocking
// otherwise.
Int Pipe_GetByteCount(PipeRef _Nonnull pPipe, PipeSide side)
{
    Int nbytes;
    
    Lock_Lock(&pPipe->lock);
    switch (side) {
        case kPipe_Reader:
            nbytes = RingBuffer_ReadableCount(&pPipe->buffer);
            break;
            
        case kPipe_Writer:
            nbytes = RingBuffer_WritableCount(&pPipe->buffer);
            break;
    }
    
    Lock_Unlock(&pPipe->lock);
    return nbytes;
}

// Returns the capacity of the pipe's byte buffer.
Int Pipe_GetCapacity(PipeRef _Nonnull pPipe)
{
    Lock_Lock(&pPipe->lock);
    const Int nbytes = pPipe->buffer.capacity;
    Lock_Unlock(&pPipe->lock);
    return nbytes;
}

// Reads up to 'nBytes' from the pipe or until all readable data has been returned.
// Which ever comes first. Blocks the caller if it is asking for more data than is
// available in the pipe and 'allowBlocking' is true. Otherwise all available data
// is read from the pipe and the amount of data read is returned. If blocking is
// allowed and the pipe has to wait for data to arrive, then the wait will time out
// after 'deadline' and a timeout error will be returned.
Int Pipe_Read(PipeRef _Nonnull pPipe, Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline)
{
    const Int nBytesToRead = __min((Int)nBytes, INT_MAX);
    Int nBytesRead = 0;
    ErrorCode err = EOK;

    if (nBytesToRead > 0) {
        Lock_Lock(&pPipe->lock);

        while (nBytesRead < nBytesToRead && pPipe->readSideState == kPipeState_Open) {
            const Int nChunkSize = RingBuffer_GetBytes(&pPipe->buffer, &pBuffer[nBytesRead], nBytesToRead - nBytesRead);

            nBytesRead += nChunkSize;
            if (nChunkSize == 0) {
                if (pPipe->writeSideState == kPipeState_Closed) {
                    err = EOK;
                    break;
                }
                
                if (allowBlocking) {
                    // Be sure to wake the writer before we go to sleep and drop the lock
                    // so that it can produce and add data for us.
                    ConditionVariable_BroadcastAndUnlock(&pPipe->writer, NULL);
                    
                    // Wait for the writer to make data available
                    if ((err = ConditionVariable_Wait(&pPipe->reader, &pPipe->lock, deadline)) != EOK) {
                        err = (nBytesRead == 0) ? EINTR : EOK;
                        break;
                    }
                } else {
                    err = (nBytesRead == 0) ? EAGAIN : EOK;
                    break;
                }
            }
        }

        Lock_Unlock(&pPipe->lock);
    }

    return (nBytesRead > 0 || err == EOK) ? nBytesRead : -err;
}

Int Pipe_Write(PipeRef _Nonnull pPipe, const Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline)
{
    const Int nBytesToWrite = __min((Int)nBytes, INT_MAX);
    Int nBytesWritten = 0;
    ErrorCode err = EOK;

    if (nBytesToWrite > 0) {
        Lock_Lock(&pPipe->lock);
        
        while (nBytesWritten < nBytesToWrite && pPipe->writeSideState == kPipeState_Open) {
            const Int nChunkSize = RingBuffer_PutBytes(&pPipe->buffer, &pBuffer[nBytesWritten], nBytesToWrite - nBytesWritten);
            
            nBytesWritten += nChunkSize;
            if (nChunkSize == 0) {
                if (pPipe->readSideState == kPipeState_Closed) {
                    err = (nBytesWritten == 0) ? EPIPE : EOK;
                    break;
                }

                if (allowBlocking) {
                    // Be sure to wake the reader before we go to sleep and drop the lock
                    // so that it can consume data and make space available to us.
                    ConditionVariable_BroadcastAndUnlock(&pPipe->reader, NULL);
                    
                    // Wait for the reader to make space available
                    if (( err = ConditionVariable_Wait(&pPipe->writer, &pPipe->lock, deadline)) != EOK) {
                        err = (nBytesWritten == 0) ? EINTR : EOK;
                        break;
                    }
                } else {
                    err = (nBytesWritten == 0) ? EAGAIN : EOK;
                    break;
                }
            }
        }

        Lock_Unlock(&pPipe->lock);
    }
    
    return (nBytesWritten > 0 || err == EOK) ? nBytesWritten : -err;
}
