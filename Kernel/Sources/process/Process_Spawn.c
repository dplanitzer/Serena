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


errno_t Process_SpawnChildProcess(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nullable pOptions, pid_t * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChild = NULL;
    bool needsUnlock = false;
    spawn_opts_t so = {0};
    DispatchQueueRef terminationNotificationQueue = NULL;

    if (pOptions) {
        so = *pOptions;
    }


    if ((so.options & kSpawn_NotifyOnProcessTermination) == kSpawn_NotifyOnProcessTermination && so.notificationQueue >= 0 && so.notificationClosure) {
        UDispatchQueueRef pQueue;

        if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, so.notificationQueue, UDispatchQueue, &pQueue)) == EOK) {
            terminationNotificationQueue = Object_RetainAs(pQueue->dispatchQueue, DispatchQueue);
            UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
        }
        else {
            throw(err);
        }
    }


    Lock_Lock(&self->lock);
    needsUnlock = true;

    uid_t childUid = self->fm.ruid;
    gid_t childGid = self->fm.rgid;
    FilePermissions childUMask = FileManager_GetFileCreationMask(&self->fm);
    if ((so.options & kSpawn_OverrideUserMask) == kSpawn_OverrideUserMask) {
        childUMask = so.umask & 0777;
    }
    if ((so.options & (kSpawn_OverrideUserId|kSpawn_OverrideGroupId)) != 0 && FileManager_GetRealUserId(&self->fm) != 0) {
        throw(EPERM);
    }
    if ((so.options & kSpawn_OverrideUserId) == kSpawn_OverrideUserId) {
        childUid = so.uid;
    }
    if ((so.options & kSpawn_OverrideGroupId) == kSpawn_OverrideGroupId) {
        childGid = so.gid;
    }

    try(Process_Create(self->pid, self->fm.fileHierarchy, childUid, childGid, self->fm.rootDirectory, self->fm.workingDirectory, childUMask, &pChild));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    if (terminationNotificationQueue) {
        pChild->terminationNotificationQueue = terminationNotificationQueue;
        pChild->terminationNotificationClosure = so.notificationClosure;
        pChild->terminationNotificationContext = so.notificationContext;

        terminationNotificationQueue = NULL;
    }

    IOChannelTable_DupFrom(&pChild->ioChannelTable, &self->ioChannelTable);

    if (so.root_dir && *so.root_dir != '\0') {
        try(Process_SetRootDirectoryPath(pChild, so.root_dir));
    }
    if (so.cw_dir && *so.cw_dir != '\0') {
        try(Process_SetWorkingDirectoryPath(pChild, so.cw_dir));
    }

    Process_AdoptChild_Locked(self, pChild);
    try(Process_Exec_Locked(pChild, path, argv, so.envp));

    ProcessManager_Register(gProcessManager, pChild);
    Object_Release(pChild);

    Lock_Unlock(&self->lock);

    if (pOutChildPid) {
        *pOutChildPid = pChild->pid;
    }

    return EOK;

catch:
    if (pChild) {
        Process_AbandonChild_Locked(self, pChild);
    }
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }

    Object_Release(pChild);
    Object_Release(terminationNotificationQueue);

    if (pOutChildPid) {
        *pOutChildPid = 0;
    }
    return err;
}

// Adopts the process with the given PID as a child. The ppid of 'pOtherProc' must
// be the PID of the receiver.
void Process_AdoptChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child)
{
    List_InsertAfterLast(&self->children, &child->siblings);
}

// Abandons the process with the given PID as a child of the receiver.
void Process_AbandonChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child)
{
    List_Remove(&self->children, &child->siblings);
}
