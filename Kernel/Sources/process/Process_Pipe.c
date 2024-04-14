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

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeChannel_Create((ObjectRef)pPipe, kOpen_Read, &rdChannel));
    try(PipeChannel_Create((ObjectRef)pPipe, kOpen_Write, &wrChannel));

    Lock_Lock(&pProc->lock);
    needsUnlock = true;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, rdChannel, pOutReadChannel));
    rdChannel = NULL;
    try(IOChannelTable_AdoptChannel(&pProc->ioChannelTable, wrChannel, pOutWriteChannel));
    wrChannel = NULL;
    Lock_Unlock(&pProc->lock);
    return EOK;

catch:
    if (rdChannel == NULL) {
        IOChannelTable_ReleaseChannel(&pProc->ioChannelTable, *pOutReadChannel);
    }
    if (needsUnlock) {
        Lock_Unlock(&pProc->lock);
    }
    IOChannel_Release(rdChannel);
    IOChannel_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}
