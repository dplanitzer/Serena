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


void Process_Init(ProcessRef _Nonnull self, ProcessRef _Locked _Nullable parent, FileHierarchyRef _Nonnull fh, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir)
{
    uid_t uid;
    gid_t gid;
    fs_perms_t umask;

    mtx_init(&self->mtx);
    AddressSpace_Init(&self->addr_space);

    self->retainCount = RC_INIT;
    self->run_state = PROC_STATE_SUSPENDED;
    self->pid = 0;

    if (parent) {
        self->ppid = parent->pid;
        self->pgrp = parent->pgrp;
        self->sid = parent->sid;

        uid = parent->fm.ruid;
        gid = parent->fm.rgid;
        umask = parent->fm.umask;
    }
    else {
        // kerneld
        self->ppid = PROC_PID_KERNELD;
        self->pgrp = 0;
        self->sid = 0;

        uid = UID_ROOT;
        gid = GID_ROOT;
        umask = fs_perms_from_octal(0022); 
    }


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
errno_t Process_CreateChild(ProcessRef _Nonnull self, const proc_spawnattr_t* _Nonnull attr, FileHierarchyRef _Nullable ovrFh, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef cp = NULL;
    InodeRef rootDir = NULL;
    InodeRef workDir = NULL;

    mtx_lock(&self->mtx);

    // Validate the spawn attributes and bail out if something is wrong
    if (attr == NULL || attr->version < _PROC_SPAWNATTR_VERSION) {
        throw(EINVAL);
    }
    if (attr->type < PROC_SPAWN_GROUP_MEMBER || attr->type > PROC_SPAWN_SESSION_LEADER) {
        throw(EINVAL);
    }
    if ((attr->flags & (_PROC_SPAFL_UID|_PROC_SPAFL_GID)) != 0 && FileManager_GetRealUserId(&self->fm) != 0) {
        throw(EPERM);
    }


    // Create the new process and let it inherit its basic state from us (the parent)
    FileHierarchyRef fh = (ovrFh) ? ovrFh : self->fm.fileHierarchy;
    rootDir = (ovrFh) ? FileHierarchy_AcquireRootDirectory(ovrFh) : Inode_Reacquire(self->fm.rootDirectory);
    workDir = (ovrFh) ? FileHierarchy_AcquireRootDirectory(ovrFh) : Inode_Reacquire(self->fm.workingDirectory);

    try(kalloc_cleared(sizeof(Process), (void**)&cp));
    Process_Init(cp, self, fh, rootDir, workDir);


    // Now override the inherited state based on the spawn attributes
    if ((attr->flags & _PROC_SPAFL_UMASK) == _PROC_SPAFL_UMASK) {
        cp->fm.umask = attr->umask & 0777;
    }
    if ((attr->flags & _PROC_SPAFL_UID) == _PROC_SPAFL_UID) {
        cp->fm.ruid = attr->uid;
    }
    if ((attr->flags & _PROC_SPAFL_GID) == _PROC_SPAFL_GID) {
        cp->fm.rgid = attr->gid;
    }

    switch (attr->type) {
        case PROC_SPAWN_GROUP_MEMBER:
            if (attr->pgrp > 0) {
                // ProcessManager_Publish() will atomically validate that the
                // group leader for this group exists
                cp->pgrp = attr->pgrp;
            }
            break;

        case PROC_SPAWN_GROUP_LEADER:
            cp->pgrp = 0;
            break;

        case PROC_SPAWN_SESSION_LEADER:
            cp->pgrp = 0;
            cp->sid = 0;
            break;
    }

    if (attr->quantum_boost > 0) {
        cp->quantum_boost = VCPU_CLAMPED_QUANTUM_BOOST(attr->quantum_boost);
    }
    if (attr->nice > 0) {
        cp->sched_nice = VCPU_CLAMPED_NICE_PRIORITY(attr->nice);
    }


catch:
    Inode_Relinquish(workDir);
    Inode_Relinquish(rootDir);

    mtx_unlock(&self->mtx);

    if (err == EOK) {
        *pOutChild = cp;
    }
    else {
        kfree(cp);
        *pOutChild = NULL;
    }

    return err;
}

// Applies the given list of spawn actions to the process.
errno_t Process_ApplyActions(ProcessRef _Nonnull self, const proc_spawn_actions_t* _Nonnull actions, ProcessRef _Nonnull parent, size_t* _Nonnull pOutFailedActionIndex)
{
    decl_try_err();

    *pOutFailedActionIndex = 0;

    if (actions->version < _PROC_SPAWN_ACTIONS_VERSION) {
        return EINVAL;
    }
    if (actions->count > __SPAWN_ACTIONS_MAX) {
        return ERANGE;
    }


    mtx_lock(&self->mtx);

    for (size_t i = 0; i < actions->count; i++) {
        const _proc_spawn_action_t* act = &actions->action[i];

        switch (act->type) {
            case _PROC_SPACT_SETCWD:
                err = FileManager_SetWorkingDirectoryPath(&self->fm, act->u.path);
                break;

            case _PROC_SPACT_SETROOTDIR:
                err = FileManager_SetRootDirectoryPath(&self->fm, act->u.path);
                break;

            case _PROC_SPACT_PASSFD:
            case _PROC_SPACT_SHAREFD:
                err = IOChannelTable_DupChannelTo(&parent->ioChannelTable, act->u.fd_map.fd, &self->ioChannelTable, act->u.fd_map.to_fd);
                break;

            default:
                err = EINVAL;
                break;
        }

        if (err != EOK) {
            *pOutFailedActionIndex = i;
            break;
        }
    }

    mtx_unlock(&self->mtx);

    return err;
}
