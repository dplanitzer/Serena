//
//  Pipe.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Pipe_h
#define Pipe_h

#include <klib/klib.h>
#include <kobj/Object.h>


// Recommended pipe buffer size
#define kPipe_DefaultBufferSize 256

enum {
    kPipeEnd_Read,
    kPipeEnd_Write
};


final_class(Pipe, Object);


// Creates the pipe with both ends closed
extern errno_t Pipe_Create(size_t bufferSize, PipeRef _Nullable * _Nonnull pOutPipe);

extern void Pipe_Open(PipeRef _Nonnull self, int end);
extern void Pipe_Close(PipeRef _Nonnull self, int end);

// Returns the number of bytes that can be read from the pipe without blocking.
extern size_t Pipe_GetNonBlockingReadableCount(PipeRef _Nonnull pPipe);

// Returns the number of bytes can be written without blocking.
extern size_t Pipe_GetNonBlockingWritableCount(PipeRef _Nonnull pPipe);

// Returns the maximum number of bytes that the pipe is capable at storing.
extern size_t Pipe_GetCapacity(PipeRef _Nonnull pPipe);

// Reads up to 'nBytes' from the pipe or until all readable data has been returned.
// Which ever comes first. Blocks the caller if it is asking for more data than
// is available in the pipe. Otherwise all available data is read from the pipe
// and the amount of data read is returned.
extern errno_t Pipe_Read(PipeRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead);

extern errno_t Pipe_Write(PipeRef _Nonnull self, const void* _Nonnull pBytes, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten);

#endif /* Pipe_h */
