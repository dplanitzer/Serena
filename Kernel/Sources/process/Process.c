//
//  Process.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "ProcessPriv.h"
#include <filemanager/FileHierarchy.h>
#include <kern/kalloc.h>

static const char*      g_systemd_argv[2] = { "/System/Commands/systemd", NULL };
static const char*      g_kernel_argv[2] = { "kerneld", NULL };
static const char*      g_kernel_env[1] = { NULL };
static pargs_t          g_kernel_pargs;
static struct Process   g_kernel_proc_storage;
ProcessRef _Nonnull gKernelProcess;



void Process_Init(ProcessRef _Nonnull self, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask)
{
    assert(ppid > 0);

    mtx_init(&self->mtx);
    AddressSpace_Init(&self->addr_space);

    self->retainCount = RC_INIT;
    self->state = PROC_STATE_RUNNING;
    self->pid = 0;
    self->ppid = ppid;
    self->pgrp = pgrp;
    self->sid = sid;

    self->vcpu_queue = LIST_INIT;
    self->next_avail_vcpuid = VCPUID_MAIN + 1;

    self->exit_reason = 0;
    
    IOChannelTable_Init(&self->ioChannelTable);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        self->waitQueueTable[i] = LIST_INIT;
    }
    self->nextAvailWaitQueueId = 0;

    wq_init(&self->sleep_queue);
    wq_init(&self->siwa_queue);
    _proc_init_default_sigroutes(self);
    FileManager_Init(&self->fm, fh, uid, gid, pRootDir, pWorkingDir, umask);
}

errno_t Process_Create(pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    ProcessRef self;
    
    err = kalloc_cleared(sizeof(Process), (void**)&self);
    if (err == EOK) {
        Process_Init(self, ppid, pgrp, sid, fh, uid, gid, pRootDir, pWorkingDir, umask);
    }

    *pOutSelf = self;
    return err;
}

static void _proc_deinit(ProcessRef _Nonnull self)
{
    IOChannelTable_Deinit(&self->ioChannelTable);
    FileManager_Deinit(&self->fm);
    _proc_destroy_sigroutes(self);

    for (size_t i = 0; i < UWQ_HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->waitQueueTable[i], ListNode, {
            u_wait_queue_t cwp = (u_wait_queue_t)pCurNode;

            uwq_destroy(cwp);
        });
    }

    wq_deinit(&self->siwa_queue);
    wq_deinit(&self->sleep_queue);
    AddressSpace_Deinit(&self->addr_space);
    self->pargs_base = NULL;

    self->pid = 0;
    self->ppid = 0;

    mtx_deinit(&self->mtx);
}

ProcessRef _Nonnull Process_Retain(ProcessRef _Nonnull self)
{
    rc_retain(&self->retainCount);
    return self;
}

void Process_Release(ProcessRef _Nullable self)
{
    if (self && rc_release(&self->retainCount)) {
        _proc_deinit(self);
        kfree(self);
    }
}

pid_t Process_GetId(ProcessRef _Nonnull self)
{
    return self->pid;
}

static void _vcpu_relinquish_self(void)
{
    vcpu_t vp = vcpu_current();

    Process_RelinquishVirtualProcessor(vp->proc, vp);
    /* NOT REACHED */
}

errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp)
{
    decl_try_err();
    const bool is_uproc = (self != gKernelProcess) ? true : false;
    vcpu_t vp = NULL;
    vcpu_acquisition_t ac;

    mtx_lock(&self->mtx);
    if (vcpu_aborting(vcpu_current())) {
        throw(EINTR);
    }

    ac.func = (VoidFunc_1)attr->func;
    ac.arg = attr->arg;
    ac.ret_func = (is_uproc) ? vcpu_uret_relinquish_self : _vcpu_relinquish_self;
    ac.kernelStackBase = NULL;
    ac.kernelStackSize = 0;
    ac.userStackSize = (is_uproc) ? __max(attr->stack_size, PROC_DEFAULT_USER_STACK_SIZE) : 0;
    ac.id = self->next_avail_vcpuid++;
    ac.groupid = attr->groupid;
    ac.schedParams = attr->sched_params;
    ac.isUser = is_uproc;

    try(vcpu_acquire(&ac, &vp));

    vp->proc = self;
    vp->udata = attr->data;
    List_InsertAfterLast(&self->vcpu_queue, &vp->owner_qe);
    self->vcpu_count++;

catch:
    mtx_unlock(&self->mtx);

    if (err == EOK) {
        *pOutVp = vp;

        if ((attr->flags & VCPU_ACQUIRE_RESUMED) == VCPU_ACQUIRE_RESUMED) {
            vcpu_resume(vp, false);
        }
    }

    return err;
}

void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    Process_DetachVirtualProcessor(self, vp);
    vcpu_relinquish(vcpu_current());
    /* NOT REACHED */
}

void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp)
{
    assert(vp->proc == self);

    mtx_lock(&self->mtx);
    List_Remove(&self->vcpu_queue, &vp->owner_qe);
    vp->proc = NULL;
    self->vcpu_count--;
    mtx_unlock(&self->mtx);
}

bool Process_ResolveExceptionHandler(ProcessRef _Nonnull self, vcpu_t _Nonnull vp, excpt_handler_t* _Nonnull handler)
{
    const excpt_handler_t* eh = NULL;

    mtx_lock(&self->mtx);
    if (vp->excpt_handler.func) {
        eh = &vp->excpt_handler;
    }
    else {
        eh = &self->excpt_handler;
    }

    if (eh) {
        *handler = *eh;
    }
    mtx_unlock(&self->mtx);

    return (eh) ? true : false;
}

errno_t Process_SetExceptionHandler(ProcessRef _Nonnull self, vcpu_t _Nonnull vp, int scope, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler)
{
    decl_try_err();

    mtx_lock(&self->mtx);
    switch (scope) {
        case EXCPT_SCOPE_VCPU:
            if (old_handler) {
                *old_handler = vp->excpt_handler;
            }
            vp->excpt_handler = *handler;
            break;

        case EXCPT_SCOPE_PROC:
            if (old_handler) {
                *old_handler = self->excpt_handler;
            }
            self->excpt_handler = *handler;
            break;

        default:
            err = EINVAL;
            break;
    }
    mtx_unlock(&self->mtx);

    return err;
}

void Process_GetSigcred(ProcessRef _Nonnull self, sigcred_t* _Nonnull cred)
{
    cred->pid = self->pid;
    cred->ppid = self->ppid;
    cred->uid = FileManager_GetRealUserId(&self->fm);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Kernel Process

void KernelProcess_Init(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf)
{
    InodeRef rootDir = FileHierarchy_AcquireRootDirectory(pRootFh);
    
    Process_Init(&g_kernel_proc_storage, PID_KERNEL, 0, 0, pRootFh, kUserId_Root, kGroupId_Root, rootDir, rootDir, perm_from_octal(0022));
    Inode_Relinquish(rootDir);

    g_kernel_pargs.version = sizeof(pargs_t);
    g_kernel_pargs.argc = 1;
    g_kernel_pargs.argv = g_kernel_argv;
    g_kernel_pargs.envp = g_kernel_env;
    g_kernel_proc_storage.pargs_base = (char*)&g_kernel_pargs;

    vcpu_t main_vp = vcpu_current();
    main_vp->proc = &g_kernel_proc_storage;
    main_vp->id = VCPUID_MAIN;
    main_vp->groupid = VCPUID_MAIN_GROUP;
    List_InsertAfterLast(&g_kernel_proc_storage.vcpu_queue, &main_vp->owner_qe);
    g_kernel_proc_storage.vcpu_count++;

    *pOutSelf = &g_kernel_proc_storage;
}

errno_t KernelProcess_SpawnSystemd(ProcessRef _Nonnull self, FileHierarchyRef _Nonnull fh)
{
    spawn_opts_t opts = (spawn_opts_t){0};

    opts.options = kSpawn_NewProcessGroup | kSpawn_NewSession;

    return Process_SpawnChild(self, g_systemd_argv[0], g_systemd_argv, &opts, fh, NULL);
}