//
//  Pipe.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Pipe_h
#define Pipe_h

#include "Foundation.h"
#include "MonotonicClock.h"

struct _Pipe;
typedef struct _Pipe* PipeRef;


// The side/end of a pipe
typedef enum {
    kPipe_Reader = 0,
    kPipe_Writer
} PipeSide;


// Recommended pipe buffer size
#define PIPE_DEFAULT_BUFFER_SIZE    256


extern ErrorCode Pipe_Create(Int bufferSize, PipeRef _Nullable * _Nonnull pOutPipe);
extern void Pipe_Destroy(PipeRef _Nullable pPipe);

extern ErrorCode Pipe_Close(PipeRef _Nonnull pPipe, PipeSide side);

extern ErrorCode Pipe_GetByteCount(PipeRef _Nonnull pPipe, PipeSide side, Int* _Nonnull pOutCount);
extern ErrorCode Pipe_GetCapacity(PipeRef _Nonnull pPipe, Int* _Nonnull pOutCapacity);

extern ErrorCode Pipe_Read(PipeRef _Nonnull pPipe, Byte* _Nonnull pBuffer, Int nBytes, Bool allowBlocking, TimeInterval deadline, Int* _Nonnull pOutNumBytesRead);
extern ErrorCode Pipe_Write(PipeRef _Nonnull pPipe, const Byte* _Nonnull pBuffer, Int nBytes, Bool allowBlocking, TimeInterval deadline, Int* _Nonnull pOutNumBytesWritten);

#endif /* Pipe_h */
