//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <filemanager/FileHierarchy.h>


static errno_t proc_create_child(ProcessRef _Locked _Nonnull self, const proc_spawn_t* _Nonnull opts, FileHierarchyRef _Nullable ovrFh, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef cp = NULL;

    uid_t ch_uid = self->fm.ruid;
    gid_t ch_gid = self->fm.rgid;
    fs_perms_t ch_umask = FileManager_GetUMask(&self->fm);
    if ((opts->options & PROC_SPAWN_UMASK) == PROC_SPAWN_UMASK) {
        ch_umask = opts->umask & 0777;
    }
    if ((opts->options & (PROC_SPAWN_UID|PROC_SPAWN_GID)) != 0 && FileManager_GetRealUserId(&self->fm) != 0) {
        throw(EPERM);
    }
    if ((opts->options & PROC_SPAWN_UID) == PROC_SPAWN_UID) {
        ch_uid = opts->uid;
    }
    if ((opts->options & PROC_SPAWN_GID) == PROC_SPAWN_GID) {
        ch_gid = opts->gid;
    }


    const pid_t ch_pgrp = (opts->options & PROC_SPAWN_GROUP_LEADER) == PROC_SPAWN_GROUP_LEADER ? 0 : self->pgrp;
    const pid_t ch_sid = (opts->options & PROC_SPAWN_SESSION_LEADER) == PROC_SPAWN_SESSION_LEADER ? 0 : self->sid;
    FileHierarchyRef fh = (ovrFh) ? ovrFh : self->fm.fileHierarchy;
    InodeRef rootDir = (ovrFh) ? FileHierarchy_AcquireRootDirectory(ovrFh) : Inode_Reacquire(self->fm.rootDirectory);
    InodeRef workDir = (ovrFh) ? FileHierarchy_AcquireRootDirectory(ovrFh) : Inode_Reacquire(self->fm.workingDirectory);

    try(Process_Create(self->pid, ch_pgrp, ch_sid, fh, ch_uid, ch_gid, rootDir, workDir, ch_umask, &cp));
    Inode_Relinquish(workDir);
    Inode_Relinquish(rootDir);


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
    if (err != EOK) {
        Process_Release(cp);
        cp = NULL;
    }

    *pOutChild = cp;

    return err;
}

errno_t Process_SpawnChild(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const proc_spawn_t* _Nonnull opts, FileHierarchyRef _Nullable ovrFh, pid_t* _Nullable pOutPid)
{
    decl_try_err();
    ProcessRef cp = NULL;

    if (*path == '\0') {
        return EINVAL;
    }
    
    // Create the child process
    mtx_lock(&self->mtx);
    if (!vcpu_is_aborting(vcpu_current())) {
        err = proc_create_child(self, opts, ovrFh, &cp);
    }
    else {
        err = EINTR;
    }
    mtx_unlock(&self->mtx);
    throw_iferr(err);


    // Prepare the executable image
    try(Process_Exec(cp, path, argv, opts->envp, false));


    // Register the new process with the process manager
    try(ProcessManager_Publish(gProcessManager, cp));
    Process_Release(cp);


    // Start the child process running
    Process_ResumeMainVirtualProcessor(cp);

catch:
    if (err == EOK) {
        if (pOutPid) {
            *pOutPid = cp->pid;
        }
    }
    else {
        Process_Release(cp);
        if (pOutPid) {
            *pOutPid = 0;
        }
    }

    return err;
}
