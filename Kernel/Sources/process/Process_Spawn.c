//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include "UDispatchQueue.h"


errno_t Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const char* _Nonnull path, const char* _Nullable argv[], const SpawnOptions* _Nullable pOptions, ProcessId * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChildProc = NULL;
    bool needsUnlock = false;
    SpawnOptions so = {0};
    DispatchQueueRef terminationNotificationQueue = NULL;

    if (pOptions) {
        so = *pOptions;
    }


    if (so.notificationQueue >= 0 && so.notificationClosure) {
        UDispatchQueueRef pQueue;

        if ((err = UResourceTable_BeginDirectResourceAccessAs(&pProc->uResourcesTable, so.notificationQueue, UDispatchQueue, &pQueue)) == EOK) {
            terminationNotificationQueue = Object_RetainAs(pQueue->dispatchQueue, DispatchQueue);
            UResourceTable_EndDirectResourceAccess(&pProc->uResourcesTable);
        }
        else {
            throw(err);
        }
    }


    Lock_Lock(&pProc->lock);
    needsUnlock = true;

    const FilePermissions childUMask = ((so.options & kSpawn_OverrideUserMask) != 0) ? (so.umask & 0777) : pProc->fileCreationMask;
    try(Process_Create(pProc->pid, pProc->fileHierarchy, pProc->realUser, pProc->rootDirectory, pProc->workingDirectory, pProc->fileCreationMask, &pChildProc));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    if (terminationNotificationQueue) {
        pChildProc->terminationNotificationQueue = terminationNotificationQueue;
        pChildProc->terminationNotificationClosure = so.notificationClosure;
        pChildProc->terminationNotificationContext = so.notificationContext;

        terminationNotificationQueue = NULL;
    }

    IOChannelTable_CopyFrom(&pChildProc->ioChannelTable, &pProc->ioChannelTable);

    if (so.root_dir && *so.root_dir != '\0') {
        try(Process_SetRootDirectoryPath(pChildProc, so.root_dir));
    }
    if (so.cw_dir && *so.cw_dir != '\0') {
        try(Process_SetWorkingDirectoryPath(pChildProc, so.cw_dir));
    }

    try(Process_AdoptChild_Locked(pProc, pChildProc->pid));
    try(Process_Exec_Locked(pChildProc, path, argv, so.envp));

    try(ProcessManager_Register(gProcessManager, pChildProc));
    Object_Release(pChildProc);

    Lock_Unlock(&pProc->lock);

    if (pOutChildPid) {
        *pOutChildPid = pChildProc->pid;
    }

    return EOK;

catch:
    if (pChildProc) {
        Process_AbandonChild_Locked(pProc, pChildProc->pid);
    }
    if (needsUnlock) {
        Lock_Unlock(&pProc->lock);
    }

    Object_Release(pChildProc);
    Object_Release(terminationNotificationQueue);

    if (pOutChildPid) {
        *pOutChildPid = 0;
    }
    return err;
}

// Adopts the process with the given PID as a child. The ppid of 'pOtherProc' must
// be the PID of the receiver.
errno_t Process_AdoptChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid)
{
    return IntArray_Add(&pProc->childPids, childPid);
}

// Abandons the process with the given PID as a child of the receiver.
void Process_AbandonChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid)
{
    IntArray_Remove(&pProc->childPids, childPid);
}
