//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"


static errno_t proc_create_child(ProcessRef _Locked _Nonnull self, const spawn_opts_t* _Nonnull opts, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef cp = NULL;

    uid_t ch_uid = self->fm.ruid;
    gid_t ch_gid = self->fm.rgid;
    mode_t ch_umask = FileManager_GetUMask(&self->fm);
    if ((opts->options & kSpawn_OverrideUserMask) == kSpawn_OverrideUserMask) {
        ch_umask = opts->umask & 0777;
    }
    if ((opts->options & (kSpawn_OverrideUserId|kSpawn_OverrideGroupId)) != 0 && FileManager_GetRealUserId(&self->fm) != 0) {
        throw(EPERM);
    }
    if ((opts->options & kSpawn_OverrideUserId) == kSpawn_OverrideUserId) {
        ch_uid = opts->uid;
    }
    if ((opts->options & kSpawn_OverrideGroupId) == kSpawn_OverrideGroupId) {
        ch_gid = opts->gid;
    }


    const pid_t ch_pgrp = (opts->options & kSpawn_NewProcessGroup) == kSpawn_NewProcessGroup ? 0 : self->pgrp;
    const pid_t ch_sid = (opts->options & kSpawn_NewSession) == kSpawn_NewSession ? 0 : self->sid;

    try(Process_Create(self->pid, ch_pgrp, ch_sid, self->fm.fileHierarchy, ch_uid, ch_gid, self->fm.rootDirectory, self->fm.workingDirectory, ch_umask, &cp));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

    IOChannelTable_DupFrom(&cp->ioChannelTable, &self->ioChannelTable);

    if (opts->root_dir && opts->root_dir[0] != '\0') {
        try(FileManager_SetRootDirectoryPath(&cp->fm, opts->root_dir));
    }
    if (opts->cw_dir && opts->cw_dir[0] != '\0') {
        try(FileManager_SetWorkingDirectoryPath(&cp->fm, opts->cw_dir));
    }

catch:
    *pOutChild = cp;
    if (err != EOK) {
        Object_Release(cp);
    }

    return err;
}

errno_t Process_SpawnChild(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, pid_t* _Nullable pOutPid)
{
    decl_try_err();
    ProcessRef cp = NULL;

    if (*path == '\0') {
        return EINVAL;
    }
    
    // Create the child process
    mtx_lock(&self->mtx);
    if (!vcpu_aborting(vcpu_current())) {
        err = proc_create_child(self, opts, &cp);
    }
    else {
        err = EINTR;
    }
    mtx_unlock(&self->mtx);
    throw_iferr(err);


    // Prepare the executable image
    try(Process_Exec(cp, path, argv, opts->envp, false));


    // Register the new process with the process manager
    try(ProcessManager_Register(gProcessManager, cp));
    List_InsertAfterLast(&self->children, &cp->siblings);
    //XXX Should remove this release because we're adopting the child and thus
    // this release is wrong. However Catalog_Unpublish leaks its ref to the
    // child process and this ends up balancing this bug here 
    Object_Release(cp);


    // Start the child process running
    Process_ResumeMainVirtualProcessor(cp);

catch:
    if (err == EOK) {
        if (pOutPid) {
            *pOutPid = cp->pid;
        }
    }
    else {
        Object_Release(cp);
        if (pOutPid) {
            *pOutPid = 0;
        }
    }

    return err;
}
