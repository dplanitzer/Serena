//
//  Process_Spawn.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include "ProcessManager.h"


// Returns the next PID available for use by a new process.
static pid_t make_unique_pid(void)
{
    static volatile AtomicInt gNextAvailablePid = 1;
    return AtomicInt_Increment(&gNextAvailablePid);
}

static errno_t proc_create_child(ProcessRef _Locked _Nonnull self, const spawn_opts_t* _Nonnull opts, ProcessRef _Nullable * _Nonnull pOutChild)
{
    decl_try_err();
    ProcessRef pChild = NULL;

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


    const pid_t child_pid = make_unique_pid();
    const pid_t child_pgrp = (opts->options & kSpawn_NewProcessGroup) == kSpawn_NewProcessGroup ? child_pid : self->pgrp;
    const pid_t child_sid = (opts->options & kSpawn_NewSession) == kSpawn_NewSession ? child_pid : self->sid;

    try(Process_Create(child_pid, self->pid, child_pgrp, child_sid, self->fm.fileHierarchy, childUid, childGid, self->fm.rootDirectory, self->fm.workingDirectory, childUMask, &pChild));


    // Note that we do not lock the child process although we're reaching directly
    // into its state. Locking isn't necessary because nobody outside this function
    // here can see the child process yet and thus call functions on it.

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

    *pOutChild = NULL;
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
    err = proc_create_child(self, opts, &cp);
    mtx_unlock(&self->mtx);
    throw_iferr(err);


    // Prepare the executable image
    try(Process_BuildExecImage(cp, path, argv, opts->envp));


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
        *pOutPid = cp->pid;
    }
    else {
        Object_Release(cp);
        *pOutPid = 0;
    }

    return err;
}
