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
#include <kern/types.h>
#include <kobj/Object.h>
#include <kpi/exception.h>
#include <kpi/proc.h>
#include <kpi/spawn.h>
#include <kpi/vcpu.h>
#include <kpi/wait.h>
#include <sched/vcpu.h>

final_class(Process, Object);


extern ProcessRef _Nonnull  gKernelProcess;

#define Process_GetCurrent() \
g_sched->running->proc


// Initializes the kerneld process and adopts the calling vcpu as kerneld's main
// vcpu.
extern void KernelProcess_Init(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf);

// Spawns systemd from the kernel process context.
extern errno_t KernelProcess_SpawnSystemd(ProcessRef _Nonnull self);



// Terminates the calling process and stores 'reason' and 'code' as the exit
// reason and code respectively. Note that this function never returns. It turns
// the calling process into a zombie and notifies the parent process so that it
// will eventually reap the zombie and free the it for good.
extern _Noreturn Process_Exit(ProcessRef _Nonnull self, int reason, int code);

// Waits for the child process with the given PID to terminate and returns the
// termination status. Returns ECHILD if the function was told to wait for a
// specific process or process group and the process or group does not exist.
extern errno_t Process_TimedJoin(ProcessRef _Nonnull self, int scope, pid_t id, int flags, const struct timespec* _Nonnull wtp, struct proc_status* _Nonnull ps);

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern errno_t Process_SpawnChild(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, pid_t* _Nullable pOutPid);

// Prepares the image of the process by replacing the current image with a new
// executable image loaded from 'execPath'. Note that this function does not
// relinquish the calling vcpu. This must be done by the caller.
// \param self the process into which the executable image should be loaded
// \param execPath path to a GemDOS executable file
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[], bool resumed);
extern void Process_ResumeMainVirtualProcessor(ProcessRef _Nonnull self);

extern errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp);
extern void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);
extern void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);

// Sends the signal 'signo' to the process 'self'. The supported signalling
// scopes are: VCPU, VCPU_GROUP and PROC.
extern errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo);

// Notifies the process that the calling vcpu has hit a CPU exception.
extern excpt_func_t _Nonnull Process_Exception(ProcessRef _Nonnull self, vcpu_t _Nonnull vp, const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ec);

// Notifies the process that the calling vcpu has returned from a user space
// exception handler. This function should pop off whatever data the exception()
// function has put on the user stack.
extern void Process_ExceptionReturn(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);
 
extern void Process_SetExceptionHandler(ProcessRef _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);


//
// Introspection
//

extern errno_t Process_Open(ProcessRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel);
extern errno_t Process_Close(ProcessRef _Nonnull self, IOChannelRef _Nonnull chan);


extern errno_t Process_GetInfo(ProcessRef _Nonnull self, proc_info_t* _Nonnull info);
extern errno_t Process_GetName(ProcessRef _Nonnull self, char* _Nonnull buf, size_t bufSize);

extern errno_t Process_Ioctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, ...);
extern errno_t Process_vIoctl(ProcessRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap);

#endif /* Process_h */
