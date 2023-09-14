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


// The end of a pipe that should be closed
typedef enum {
    kPipeClosing_Reader = 0,
    kPipeClosing_Writer,
    kPipeClosing_Both
} PipeClosing;


// Recommended pipe buffer size
#define PIPE_DEFAULT_BUFFER_SIZE    256


extern ErrorCode Pipe_Create(Int bufferSize, PipeRef _Nullable * _Nonnull pOutPipe);
extern void Pipe_Destroy(PipeRef _Nullable pPipe);

extern void Pipe_Close(PipeRef _Nonnull pPipe, PipeClosing mode);

// Returns the number of bytes that can be read from the pipe without blocking.
extern Int Pipe_GetNonBlockingReadableCount(PipeRef _Nonnull pPipe);

// Returns the number of bytes can be written without blocking.
extern Int Pipe_GetNonBlockingWritableCount(PipeRef _Nonnull pPipe);

// Returns the maximum number of bytes that the pipe is capable at storing.
extern Int Pipe_GetCapacity(PipeRef _Nonnull pPipe);

extern Int Pipe_Read(PipeRef _Nonnull pPipe, Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline);
extern Int Pipe_Write(PipeRef _Nonnull pPipe, const Byte* _Nonnull pBuffer, ByteCount nBytes, Bool allowBlocking, TimeInterval deadline);

#endif /* Pipe_h */
