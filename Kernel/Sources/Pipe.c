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
#include "Heap.h"
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
PipeRef _Nullable Pipe_Create(Int bufferSize)
{
    assert(bufferSize > 0);
    
    PipeRef pPipe = (PipeRef)kalloc(sizeof(Pipe), 0);
    FailNULL(pPipe);
    
    Lock_Init(&pPipe->lock);
    ConditionVariable_Init(&pPipe->reader);
    ConditionVariable_Init(&pPipe->writer);
    FailFalse(RingBuffer_Init(&pPipe->buffer, bufferSize));
    pPipe->readSideState = kPipeState_Open;
    pPipe->writeSideState = kPipeState_Open;

    return pPipe;
    
failed:
    Pipe_Destroy(pPipe);
    return NULL;
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

    ConditionVariable_Broadcast(&pPipe->reader, &pPipe->lock);
    ConditionVariable_Broadcast(&pPipe->writer, &pPipe->lock);
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
    Int nbytes;
    
    Lock_Lock(&pPipe->lock);
    nbytes = pPipe->buffer.capacity;
    Lock_Unlock(&pPipe->lock);
    
    return nbytes;
}

#define DOPRINT 1
// Reads up to 'nBytes' from the pipe or until all readable data has been returned.
// Which ever comes first. Blocks the caller if it is asking for more data than is
// available in the pipe and 'allowBlocking' is true. Otherwise all available data
// is read from the pipe and the amount of data read is returned. If blocking is
// allowed and the pipe has to wait for data to arrive, then the wait will time out
// after 'deadline' and a timeout error will be returned.
Int Pipe_Read(PipeRef _Nonnull pPipe, ErrorCode* _Nonnull pError, Byte* _Nonnull pBuffer, Int nBytes, Bool allowBlocking, TimeInterval deadline)
{
    Int nBytesRead = 0;

    *pError = EOK;
    if (nBytes > 0) {
        Lock_Lock(&pPipe->lock);
        
        while (nBytesRead < nBytes && pPipe->readSideState == kPipeState_Open) {
            const Int nChunkSize = RingBuffer_GetBytes(&pPipe->buffer, &pBuffer[nBytesRead], nBytes - nBytesRead);

            nBytesRead += nChunkSize;
            if (nChunkSize == 0) {
                if (pPipe->writeSideState == kPipeState_Closed) {
                    // Only way for us to observe here that the writer closed is that this happend
                    // while we were waiting in a previous loop iteration.
                    break;
                }
                
                if (allowBlocking) {
                    // Be sure to wake the writer before we go to sleep and drop the lock
                    // so that it can produce and add data for us.
                    ConditionVariable_Broadcast(&pPipe->writer, NULL);
                    
                    // Wait for the writer to make data available
                    const ErrorCode err = ConditionVariable_Wait(&pPipe->reader, &pPipe->lock, deadline);
                    if (err != EOK) {
                        *pError = err;
                        break;
                    }
                } else {
                    *pError = EAGAIN;
                    break;
                }
            }
        }

        // Make sure that the writer is awake so that it can produce new data
        ConditionVariable_Broadcast(&pPipe->writer, &pPipe->lock);
        
        
        // Return 0 bytes and EOK on EOF
        if (nBytesRead == 0 && (pPipe->writeSideState == kPipeState_Closed || pPipe->readSideState == kPipeState_Closed)) {
            *pError = EOK;
        }
    }
    
    return nBytesRead;
}

Int Pipe_Write(PipeRef _Nonnull pPipe, ErrorCode* _Nonnull pError, const Byte* _Nonnull pBuffer, Int nBytes, Bool allowBlocking, TimeInterval deadline)
{
    Int nBytesWritten = 0;

    *pError = EOK;
    if (nBytes > 0) {
        Lock_Lock(&pPipe->lock);
        
        while (nBytesWritten < nBytes && pPipe->writeSideState == kPipeState_Open) {
            const Int nChunkSize = RingBuffer_PutBytes(&pPipe->buffer, &pBuffer[nBytesWritten], nBytes - nBytesWritten);
            
            nBytesWritten += nChunkSize;
            if (nChunkSize == 0) {
                if (pPipe->readSideState == kPipeState_Closed) {
                    // Only way for us to observe here that the writer closed is that this happend
                    // while we were waiting in a previous loop iteration.
                    break;
                }

                if (allowBlocking) {
                    // Be sure to wake the reader before we go to sleep and drop the lock
                    // so that it can consume data and make space available for us.
                    ConditionVariable_Broadcast(&pPipe->reader, NULL);
                    
                    // Wait for the reader to make space available
                    const ErrorCode err = ConditionVariable_Wait(&pPipe->writer, &pPipe->lock, deadline);
                    if (err != EOK) {
                        *pError = err;
                        break;
                    }
                } else {
                    *pError = EAGAIN;
                    break;
                }
            }
        }
        
        // Make sure that the reader is awake so that it can consume the new data
        ConditionVariable_Broadcast(&pPipe->reader, &pPipe->lock);
        
        
        // Return 0 bytes and EOK on EOF
        if (nBytesWritten == 0 && (pPipe->readSideState == kPipeState_Closed || pPipe->writeSideState == kPipeState_Closed)) {
            *pError = EPIPE;
        }
    }
    
    return nBytesWritten;
}
