//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"
#include <assert.h>
#include <filemanager/FileHierarchy.h>
#include <kern/kalloc.h>


void Process_Init(ProcessRef _Nonnull self, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, fs_perms_t umask)
{
    assert(ppid > 0);

    mtx_init(&self->mtx);
    AddressSpace_Init(&self->addr_space);

    self->retainCount = RC_INIT;
    self->run_state = PROC_STATE_RESUMED;
    self->pid = 0;
    self->ppid = ppid;
    self->pgrp = pgrp;
    self->sid = sid;

    self->vcpu_queue = DEQUE_INIT;
    self->next_avail_vcpuid = VCPUID_MAIN + 1;

    self->exit_reason = 0;
    
    IOChannelTable_Init(&self->ioChannelTable);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        self->waitQueueTable[i] = DEQUE_INIT;
    }
    self->nextAvailWaitQueueId = 0;

    clock_gettime(g_mono_clock, &self->creation_time);

    wq_init(&self->clk_wait_queue);
    wq_init(&self->siwa_queue);
   
    _proc_init_default_sigroutes(self);

    FileManager_Init(&self->fm, fh, uid, gid, pRootDir, pWorkingDir, umask);
}

// Creates a new child process and publishes it to the process manager. This
// function returns a strong reference to the new process. The caller should
// release this strong reference when no longer needed. Note that the process
// manager maintains a strong reference to all living processes. This reference
// keeps them alive.
errno_t Process_CreateChild(ProcessRef _Nonnull self, const proc_spawn_t* _Nonnull opts, FileHierarchyRef _Nullable ovrFh, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef cp = NULL;

    mtx_lock(&self->mtx);

    self->flags |= PROC_FLAG_INCUBATING;
    
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

    try(kalloc_cleared(sizeof(Process), (void**)&cp));
    Process_Init(cp, self->pid, ch_pgrp, ch_sid, fh, ch_uid, ch_gid, rootDir, workDir, ch_umask);

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
    mtx_unlock(&self->mtx);

    if (err == EOK) {
        // Register the new process with the process manager. This assigns a
        // unique PID to our new process
        ProcessManager_Publish(gProcessManager, cp);
        *pOutChild = cp;
    }
    else {
        Process_Release(cp);
        *pOutChild = NULL;
    }

    return err;
}
