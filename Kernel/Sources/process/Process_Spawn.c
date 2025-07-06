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


static errno_t proc_create_child(ProcessRef _Locked _Nonnull self, const spawn_opts_t* _Nonnull opts, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef pChild = NULL;
    DispatchQueueRef terminationNotificationQueue = NULL;

    if ((opts->options & kSpawn_NotifyOnProcessTermination) == kSpawn_NotifyOnProcessTermination && opts->notificationQueue >= 0 && opts->notificationClosure) {
        UDispatchQueueRef pQueue;

        if ((err = UResourceTable_BeginDirectResourceAccessAs(&self->uResourcesTable, opts->notificationQueue, UDispatchQueue, &pQueue)) == EOK) {
            terminationNotificationQueue = Object_RetainAs(pQueue->dispatchQueue, DispatchQueue);
            UResourceTable_EndDirectResourceAccess(&self->uResourcesTable);
        }
        else {
            throw(err);
        }
    }


    uid_t childUid = self->fm.ruid;
    gid_t childGid = self->fm.rgid;
    mode_t childUMask = FileManager_GetUMask(&self->fm);
    if ((opts->options & kSpawn_OverrideUserMask) == kSpawn_OverrideUserMask) {
        childUMask = opts->umask & 0777;
    }
    if ((opts->options & (kSpawn_OverrideUserId|kSpawn_OverrideGroupId)) != 0 && FileManager_GetRealUserId(&self->fm) != 0) {
        throw(EPERM);
    }
    if ((opts->options & kSpawn_OverrideUserId) == kSpawn_OverrideUserId) {
        childUid = opts->uid;
    }
    if ((opts->options & kSpawn_OverrideGroupId) == kSpawn_OverrideGroupId) {
        childGid = opts->gid;
    }

    try(Process_Create(self->pid, self->fm.fileHierarchy, childUid, childGid, self->fm.rootDirectory, self->fm.workingDirectory, childUMask, &pChild));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    if (terminationNotificationQueue) {
        pChild->terminationNotificationQueue = terminationNotificationQueue;
        pChild->terminationNotificationClosure = opts->notificationClosure;
        pChild->terminationNotificationContext = opts->notificationContext;

        terminationNotificationQueue = NULL;
    }

    IOChannelTable_DupFrom(&pChild->ioChannelTable, &self->ioChannelTable);

    if (opts->root_dir && opts->root_dir[0] != '\0') {
        try(FileManager_SetRootDirectoryPath(&pChild->fm, opts->root_dir));
    }
    if (opts->cw_dir && opts->cw_dir[0] != '\0') {
        try(FileManager_SetWorkingDirectoryPath(&pChild->fm, opts->cw_dir));
    }

    *pOutChild = pChild;

    return EOK;

catch:
    Object_Release(pChild);
    Object_Release(terminationNotificationQueue);

    *pOutChild = NULL;
    return err;
}

errno_t Process_SpawnChildProcess(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, pid_t * _Nullable pOutChildPid)
{
    decl_try_err();
    ProcessRef pChild = NULL;
    bool doTermChild = false;

    if (*path == '\0') {
        return EINVAL;
    }
    
    Lock_Lock(&self->lock);

    err = proc_create_child(self, opts, &pChild);
    if (err == EOK) {
        Process_AdoptChild_Locked(self, pChild);
        ProcessManager_Register(gProcessManager, pChild);
        Object_Release(pChild);

        Process_Publish(pChild);
        
        err = Process_Exec(pChild, path, argv, opts->envp);
        if (err != EOK) {
            doTermChild = true;
        }
    }

    Lock_Unlock(&self->lock);

    if (doTermChild) {
        Process_Terminate(pChild, 127);
        pChild = NULL;
    }

    if (pOutChildPid) {
        *pOutChildPid = (pChild) ? pChild->pid : 0;
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
