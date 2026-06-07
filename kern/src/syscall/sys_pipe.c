//
//  sys_pipe.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/6/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <handler/PipeHandler.h>
#include <ipc/Pipe.h>
#include <kpi/pipe.h>


SYSCALL_1(pipe_create, int* _Nonnull fds)
{
    decl_try_err();
    ProcessRef pp = vp->proc;
    PipeRef pPipe = NULL;
    HandlerRef rdHnd = NULL, wrHnd = NULL;
    bool needsUnlock = false;

    try(Pipe_Create(kPipe_DefaultBufferSize, &pPipe));
    try(PipeHandler_Create(pPipe, O_RDONLY, &rdHnd));
    try(PipeHandler_Create(pPipe, O_WRONLY, &wrHnd));

    mtx_lock(&pp->mtx);
    needsUnlock = true;
    try(HandlerTable_AdoptHandler(&pp->HandlerTable, rdHnd, &pa->fds[PIPE_FD_READ]));
    rdHnd = NULL;
    try(HandlerTable_AdoptHandler(&pp->HandlerTable, wrHnd, &pa->fds[PIPE_FD_WRITE]));
    wrHnd = NULL;
    mtx_unlock(&pp->mtx);
    return EOK;

catch:
    if (wrHnd == NULL) {
        HandlerTable_ReleaseHandler(&pp->HandlerTable, pa->fds[PIPE_FD_READ]);
    }
    if (needsUnlock) {
        mtx_unlock(&pp->mtx);
    }
    Handler_Release(rdHnd);
    Handler_Release(wrHnd);
    Object_Release(pPipe);
    return err;
}
