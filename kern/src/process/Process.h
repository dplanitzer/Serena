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
#include <kpi/process.h>
#include <kpi/spawn.h>
#include <kpi/types.h>
#include <kpi/vcpu.h>
#include <sched/vcpu.h>
#include <security/SecurityManager.h>


#define Process_GetCurrent() \
g_sched->running->proc


extern ProcessRef _Nonnull Process_Retain(ProcessRef _Nonnull self);
extern void Process_Release(ProcessRef _Nullable self);


extern pid_t Process_GetId(ProcessRef _Nonnull self);
extern pid_t Process_GetParentId(ProcessRef _Nonnull self);
extern pid_t Process_GetSessionId(ProcessRef _Nonnull self);
extern uid_t Process_GetUserId(ProcessRef _Nonnull self);

extern void Process_GetSigcred(ProcessRef _Nonnull self, sigcred_t* _Nonnull cred);

// Returns the current process state.
extern int Process_GetState(ProcessRef _Nonnull self);

// Terminates the calling process and stores 'reason' and 'arg' as the exit
// reason and associated argument (i.e. exit code) respectively. Note that this
// function never returns. It turns the calling process into a zombie and
// notifies the parent process so that it will eventually reap the zombie and
// free the it for good.
extern _Noreturn void Process_Terminate(ProcessRef _Nonnull self, int reason, int arg);

// Waits for the process 'self' to enter the state 'wstate'. Does not block the
// caller and returns immediately with EAGAIN if 'flags' has NONBLOCKING set and
// the current process state is not 'state'; otherwise returns EOK and a suitable
// status report. Returns ECHILD if the requested child process does not exist.
extern errno_t Process_WaitForState(ProcessRef _Nonnull self, int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res);

// Replaces the current executable image of the process with a new executable
// image loaded from 'execPath' and starts it executing right away. Note that
// this function does not relinquish the calling vcpu. This must be done by the
// caller.
// \param self the process into which the executable image should be loaded
// \param execPath path to an executable file
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[]);

extern errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, const _vcpu_acquire_attr_t* _Nonnull attr, vcpu_t _Nullable * _Nonnull pOutVp);
extern void Process_RelinquishVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);
extern void Process_DetachVirtualProcessor(ProcessRef _Nonnull self, vcpu_t _Nonnull vp);


extern errno_t Process_SetExceptionHandler(ProcessRef _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);
extern void Process_GetExceptionHandler(ProcessRef _Nonnull self, excpt_handler_t* _Nonnull handler);


// Sends the signal 'signo' to the process 'self'. The supported signalling
// scopes are: VCPU, VCPU_GROUP and PROC.
extern errno_t Process_SendSignal(ProcessRef _Nonnull self, int scope, id_t id, int signo);

// Adds or deletes a route for the signal 'signo'.
extern errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int scope, id_t id);


// Sets/gets a scheduling parameter
extern errno_t Process_GetSchedParam(ProcessRef _Nonnull self, int type, int* _Nonnull param);
extern errno_t Process_SetSchedParam(ProcessRef _Nonnull self, int type, const int* _Nonnull param);


// Returns a copy of a specific process property.
extern errno_t Process_GetProperty(ProcessRef _Nonnull self, int flavor, char* _Nonnull buf, size_t bufSize);

// Returns a copy of the receiver's information.
extern errno_t Process_GetInfo(ProcessRef _Nonnull self, int flavor, proc_info_ref _Nonnull info);

// Returns an array of vcpuid_t's corresponding to the currently acquired vcpus.
extern errno_t Process_GetVirtualProcessorIds(ProcessRef _Nonnull self, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore);

extern void Process_Suspend(ProcessRef _Nonnull self);
extern void Process_Resume(ProcessRef _Nonnull self);

#endif /* Process_h */
