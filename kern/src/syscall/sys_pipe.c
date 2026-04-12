//
//  sys_pipe.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ipc/PipeChannel.h>
#include <kpi/pipe.h>


SYSCALL_1(pipe_create, int* _Nonnull fds)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    PipeRef pPipe = NULL;
    IOChannelRef rdChannel = NULL, wrChannel = NULL;
    bool needsUnlock = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeChannel_Create(pPipe, O_RDONLY, &rdChannel));
    try(PipeChannel_Create(pPipe, O_WRONLY, &wrChannel));

    mtx_lock(&pp->mtx);
    needsUnlock = true;
    try(IOChannelTable_AdoptChannel(&pp->ioChannelTable, rdChannel, &pa->fds[PIPE_FD_READ]));
    rdChannel = NULL;
    try(IOChannelTable_AdoptChannel(&pp->ioChannelTable, wrChannel, &pa->fds[PIPE_FD_WRITE]));
    wrChannel = NULL;
    mtx_unlock(&pp->mtx);
    return EOK;

catch:
    if (wrChannel == NULL) {
        IOChannelTable_ReleaseChannel(&pp->ioChannelTable, pa->fds[PIPE_FD_READ]);
    }
    if (needsUnlock) {
        mtx_unlock(&pp->mtx);
    }
    IOChannel_Release(rdChannel);
    IOChannel_Release(wrChannel);
    Object_Release(pPipe);
    return err;
}
