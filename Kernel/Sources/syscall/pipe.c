//
//  pipe.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ipc/PipeChannel.h>
#include <kpi/fcntl.h>


SYSCALL_2(mkpipe, int* _Nonnull pOutReadChannel, int* _Nonnull pOutWriteChannel)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    PipeRef pPipe = NULL;
    IOChannelRef rdChannel = NULL, wrChannel = NULL;
    bool needsUnlock = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeChannel_Create(pPipe, O_RDONLY, &rdChannel));
    try(PipeChannel_Create(pPipe, O_WRONLY, &wrChannel));

    Lock_Lock(&pp->lock);
    needsUnlock = true;
    try(IOChannelTable_AdoptChannel(&pp->ioChannelTable, rdChannel, pa->pOutReadChannel));
    rdChannel = NULL;
    try(IOChannelTable_AdoptChannel(&pp->ioChannelTable, wrChannel, pa->pOutWriteChannel));
    wrChannel = NULL;
    Lock_Unlock(&pp->lock);
    return EOK;

catch:
    if (rdChannel == NULL) {
        IOChannelTable_ReleaseChannel(&pp->ioChannelTable, *(pa->pOutReadChannel));
    }
    if (needsUnlock) {
        Lock_Unlock(&pp->lock);
    }
    IOChannel_Release(rdChannel);
    IOChannel_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}
