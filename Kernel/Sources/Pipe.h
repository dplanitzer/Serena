//
//  Pipe.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/9/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Pipe_h
#define Pipe_h

#include <IOResource.h>


// Recommended pipe buffer size
#define kPipe_DefaultBufferSize 256


OPAQUE_CLASS(Pipe, IOResource);
typedef struct _PipeMethodTable {
    IOResourceMethodTable   super;
} PipeMethodTable;


extern errno_t Pipe_Create(size_t bufferSize, PipeRef _Nullable * _Nonnull pOutPipe);

// Returns the number of bytes that can be read from the pipe without blocking.
extern size_t Pipe_GetNonBlockingReadableCount(PipeRef _Nonnull pPipe);

// Returns the number of bytes can be written without blocking.
extern size_t Pipe_GetNonBlockingWritableCount(PipeRef _Nonnull pPipe);

// Returns the maximum number of bytes that the pipe is capable at storing.
extern size_t Pipe_GetCapacity(PipeRef _Nonnull pPipe);

#endif /* Pipe_h */
