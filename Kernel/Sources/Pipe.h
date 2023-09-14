//
//  Pipe.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Pipe_h
#define Pipe_h

#include <klib/klib.h>

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

extern void Pipe_Close(PipeRef _Nonnull pPipe, PipeSide side);

extern Int Pipe_GetByteCount(PipeRef _Nonnull pPipe, PipeSide side);
extern Int Pipe_GetCapacity(PipeRef _Nonnull pPipe);

extern Int Pipe_Read(PipeRef _Nonnull pPipe, Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline);
extern Int Pipe_Write(PipeRef _Nonnull pPipe, const Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline);

#endif /* Pipe_h */
