//
//  Process_IPC.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <ipc/PipeChannel.h>



// Creates an anonymous pipe.
errno_t Process_CreatePipe(ProcessRef _Nonnull self, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel)
{
    decl_try_err();
    PipeRef pPipe = NULL;
    IOChannelRef rdChannel = NULL, wrChannel = NULL;
    bool needsUnlock = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeChannel_Create(pPipe, O_RDONLY, &rdChannel));
    try(PipeChannel_Create(pPipe, O_WRONLY, &wrChannel));

    Lock_Lock(&self->lock);
    needsUnlock = true;
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, rdChannel, pOutReadChannel));
    rdChannel = NULL;
    try(IOChannelTable_AdoptChannel(&self->ioChannelTable, wrChannel, pOutWriteChannel));
    wrChannel = NULL;
    Lock_Unlock(&self->lock);
    return EOK;

catch:
    if (rdChannel == NULL) {
        IOChannelTable_ReleaseChannel(&self->ioChannelTable, *pOutReadChannel);
    }
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    IOChannel_Release(rdChannel);
    IOChannel_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}
