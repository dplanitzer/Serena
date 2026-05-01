//
//  Process_Status.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"


static ProcessRef _Nullable _find_matching_zombie(ProcessRef _Nonnull self, int match, pid_t id, bool* _Nonnull pOutExists)
{
    switch (match) {
        case STATUS_OF_PID:
            return ProcessManager_CopyZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case STATUS_OF_GROUP:
            return ProcessManager_CopyGroupZombieOfParent(gProcessManager, self->pid, id, pOutExists);

        case STATUS_OF_ANY:
            return ProcessManager_CopyAnyZombieOfParent(gProcessManager, self->pid, pOutExists);

        default:
            return NULL;
    }
}

errno_t Process_GetStatus(ProcessRef _Nonnull self, int match, pid_t id, int flags, proc_status_t* _Nonnull status)
{
    decl_try_err();
    ProcessRef zp = NULL;

    switch (match) {
        case STATUS_OF_PID:
        case STATUS_OF_GROUP:
        case STATUS_OF_ANY:
            break;

        default:
            return EINVAL;
    }

    if ((flags & ~(STATUS_NONBLOCKING)) != 0) {
        return EINVAL;
    }


    for (;;) {
        bool exists = false;

        zp = _find_matching_zombie(self, match, id, &exists);

        if (zp) {
            break;
        }

        if (!exists) {
            return ECHILD;
        }
        
        if ((flags & STATUS_NONBLOCKING) == STATUS_NONBLOCKING) {
            return EAGAIN;
        }


        sigset_t hot_sigs = sig_bit(SIG_CHILD);
        int signo;

        err = vcpu_wait_for_signal(&self->siwa_queue, &hot_sigs, &signo);
        if (err != EOK) {
            return err;
        }
    }


    status->pid = zp->pid;
    status->state = PROC_STATE_TERMINATED;
    status->reason = zp->exit_reason;
    status->u.status = zp->exit_code;

    ProcessManager_Unpublish(gProcessManager, zp);
    Process_Release(zp); // necessary because of the _find_matching_zombie() above

    return EOK;
}

// Let our parent know that we're dead now and that it should remember us by
// commissioning a beautiful tombstone for us.
void _proc_notify_parent(ProcessRef _Nonnull self)
{
    if (!Process_IsRoot(self)) {
        sigcred_t sc;

        Process_GetSigcred(self, &sc);
        ProcessManager_SendSignal(gProcessManager, &sc, SIG_SCOPE_PROC, self->ppid, SIG_CHILD);
    }
}
