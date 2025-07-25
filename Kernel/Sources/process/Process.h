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


extern ProcessRef _Nonnull  gRootProcess;

#define Process_GetCurrent() \
g_sched->running->proc


// Creates the root process which is the first process of the OS.
extern errno_t RootProcess_Create(FileHierarchyRef _Nonnull pRootFh, ProcessRef _Nullable * _Nonnull pOutSelf);


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
extern errno_t Process_SpawnChildProcess(ProcessRef _Nonnull self, const char* _Nonnull path, const char* _Nullable argv[], const spawn_opts_t* _Nonnull opts, pid_t * _Nullable pOutChildPid);

// Loads an executable from the given executable file into the process address
// space. This is only meant to get the root process going.
// \param self the process into which the executable image should be loaded
// \param pExecPath path to a GemDOS executable file
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[]);

extern errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpuid_t* _Nonnull idp);
extern void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);
extern void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);

// Sends the signal 'signo' to the process 'self'. The supported signalling
// scopes are: VCPU, VCPU_GROUP and PROC.
extern errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo);

// Notifies the process that the calling vcpu has hit a CPU exception.
extern excpt_handler_t _Nonnull Process_Exception(ProcessRef _Nonnull self, const excpt_info_t* _Nonnull ei, excpt_ctx_t* _Nonnull ec);

extern excpt_handler_t Process_SetExceptionHandler(ProcessRef _Nonnull self, excpt_handler_t _Nullable handler);


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
