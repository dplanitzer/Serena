//
//  Process_Pipe.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "Pipe.h"
#include "PipeChannel.h"



// Creates an anonymous pipe.
errno_t Process_CreatePipe(ProcessRef _Nonnull pProc, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel)
{
    decl_try_err();
    PipeRef pPipe = NULL;
    IOChannelRef rdChannel = NULL, wrChannel = NULL;
    bool needsUnlock = false;
    bool isReadChannelRegistered = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeChannel_Create((ObjectRef)pPipe, kOpen_Read, &rdChannel));
    try(PipeChannel_Create((ObjectRef)pPipe, kOpen_Write, &wrChannel));

    Lock_Lock(&pProc->lock);
    needsUnlock = true;
    try(Process_RegisterIOChannel_Locked(pProc, rdChannel, pOutReadChannel));
    isReadChannelRegistered = true;
    try(Process_RegisterIOChannel_Locked(pProc, wrChannel, pOutWriteChannel));
    Object_Release(rdChannel);
    Object_Release(wrChannel);
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    if (isReadChannelRegistered) {
        Process_UnregisterIOChannel(pProc, *pOutReadChannel, &rdChannel);
        Object_Release(rdChannel);
    }
    if (needsUnlock) {
        Lock_Unlock(&pProc->lock);
    }
    Object_Release(rdChannel);
    Object_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}
