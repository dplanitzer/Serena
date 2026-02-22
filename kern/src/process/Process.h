//
//  Process.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include <stdarg.h>
#include <stddef.h>
#include <kobj/Any.h>
#include <kpi/exception.h>
#include <kpi/proc.h>
#include <kpi/spawn.h>
#include <kpi/types.h>
#include <kpi/vcpu.h>
#include <kpi/wait.h>
#include <sched/vcpu.h>
#include <security/SecurityManager.h>


extern ProcessRef _Nonnull  gKernelProcess;

#define PID_KERNEL  1

#define Process_GetCurrent() \
g_sched->running->proc


// Initializes the kerneld process and adopts the calling vcpu as kerneld's main
// vcpu.
extern void KernelProcess_Init(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf);

// Spawns systemd from the kernel process context.
extern errno_t KernelProcess_SpawnSystemd(ProcessRef _Nonnull self, FileHierarchyRef _Nonnull fh);


extern ProcessRef _Nonnull Process_Retain(ProcessRef _Nonnull self);
extern void Process_Release(ProcessRef _Nullable self);


extern pid_t Process_GetId(ProcessRef _Nonnull self);
extern void Process_GetSigcred(ProcessRef _Nonnull self, sigcred_t* _Nonnull cred);
extern errno_t Process_GetArgv0(ProcessRef _Nonnull self, char* _Nonnull buf, size_t buflen);

// Returns the current process state. The returned state is inexact in the sense
// that the returned state will be RUNNING even if all vcpus are in waiting or
// suspended state.
extern int Process_GetInexactState(ProcessRef _Nonnull self);

// Terminates the calling process and stores 'reason' and 'code' as the exit
// reason and code respectively. Note that this function never returns. It turns
// the calling process into a zombie and notifies the parent process so that it
// will eventually reap the zombie and free the it for good.
extern _Noreturn void Process_Exit(ProcessRef _Nonnull self, int reason, int code);

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if the function was told to wait for a
// specific process or process group and the process or group does not exist.
extern errno_t Process_TimedJoin(ProcessRef _Nonnull self, int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps);

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern errno_t Process_SpawnChild(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, FileHierarchyRef _Nullable ovrFh, pid_t* _Nullable pOutPid);

// Prepares the image of the process by replacing the current image with a new
// executable image loaded from 'execPath'. Note that this function does not
// relinquish the calling vcpu. This must be done by the caller.
// \param self the process into which the executable image should be loaded
// \param execPath path to a GemDOS executable file
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[], bool resumed);
extern void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self);

extern errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp);
extern void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);
extern void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);

// Sends the signal 'signo' to the process 'self'. The supported signalling
// scopes are: VCPU, VCPU_GROUP and PROC.
extern errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo);

// Adds or deletes a route for the signal 'signo'.
extern errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int scope, id_t id);


// Finds out which exception handler should be used to handle a CPU exception
// and returns true if such a handler exists; otherwise false.
extern bool Process_GetExceptionHandler(ProcessRef _Nonnull self, vcpu_t _Nonnull vp, excpt_handler_t* _Nonnull handler);
extern errno_t Process_SetExceptionHandler(ProcessRef _Nonnull self, vcpu_t _Nonnull vp, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);


//
// Introspection
//

extern errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);


extern errno_t Process_GetInfo(ProcessRef _Nonnull self, proc_info_t* _Nonnull info);
extern errno_t Process_GetName(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize);

extern errno_t Process_Ioctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, ...);
extern errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap);

#endif /* Process_h */
