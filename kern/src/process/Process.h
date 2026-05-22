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
#include <kern/signal.h>
#include <kobj/Any.h>
#include <kpi/exception.h>
#include <kpi/process.h>
#include <kpi/proc_wait.h>
#include <kpi/proc_spawn.h>
#include <kpi/types.h>
#include <kpi/vcpu.h>
#include <sched/vcpu.h>
#include <security/perm_check.h>


#define Process_GetCurrent() \
g_sched->running->proc


extern ProcessRef _Nonnull Process_Retain(ProcessRef _Nonnull self);
extern void Process_Release(ProcessRef _Nullable self);


extern pid_t Process_GetId(ProcessRef _Nonnull self);
extern pid_t Process_GetParentId(ProcessRef _Nonnull self);
extern pid_t Process_GetSessionId(ProcessRef _Nonnull self);
extern uid_t Process_GetUserId(ProcessRef _Nonnull self);


// Terminates the calling process and stores 'reason' and 'arg' as the exit
// reason and associated argument (i.e. exit code) respectively. Note that this
// function never returns. It turns the calling process into a zombie and
// notifies the parent process so that it will eventually reap the zombie and
// free the it for good.
// @Entry Condition: calling vcpu must belong to the process 'self'
extern _Noreturn void Process_Terminate(ProcessRef _Nonnull self, int reason, int arg);

extern void Process_Stop(ProcessRef _Nonnull self, int reason, int arg, bool notify_parent);
extern void Process_Continue(ProcessRef _Nonnull self, int reason, int arg, bool notify_parent);


// Checks whether the current process state matches the provide criteria 'mstate'
// If so then the state and the reason for transitioning to this state is returned
// and we note the fact that the status has been consumed. Otherwise EAGAIN is
// returned.
extern errno_t Process_GetMatchingState(ProcessRef _Nonnull self, int mstate, proc_waitres_t* _Nonnull res);

// Waits for the process 'self' to enter the state 'wstate'. Does not block the
// caller and returns immediately with EAGAIN if 'flags' has NONBLOCKING set and
// the current process state is not 'state'; otherwise returns EOK and a suitable
// status report. Returns ECHILD if the requested child process does not exist.
extern errno_t Process_WaitForState(ProcessRef _Nonnull self, int wstate, int match, pid_t id, int flags, proc_waitres_t* _Nonnull res);

// Loads the executable at 'execPath' and prepares it for execution. The existing
// vcpus are relinquished, signal routes are freed and all open descriptors are
// closed if this is a replacement exec. The current (calling) vcpu becomes the
// new main vcpu in this case. If on the other hand this is the first exec() call
// on a new process then this function acquires a new main vcpu.
extern errno_t Process_Exec(ProcessRef _Nonnull self, const char* _Nonnull execPath, const char* _Nullable argv[], const char* _Nullable env[]);

extern errno_t Process_AcquireVirtualProcessor(ProcessRef _Nonnull self, vcpu_func_t _Nonnull func, void* _Nullable arg, const vcpu_attr_t* _Nonnull attr, intptr_t udata, vcpu_t _Nullable * _Nonnull pOutVp);
extern _Noreturn void Process_RelinquishCurrentVirtualProcessor(ProcessRef _Nonnull self);


extern errno_t Process_SetExceptionHandler(ProcessRef _Nonnull self, const excpt_handler_t* _Nullable handler, excpt_handler_t* _Nullable old_handler);
extern void Process_GetExceptionHandler(ProcessRef _Nonnull self, excpt_handler_t* _Nonnull handler);


// Sends a signal to 'self' or another process. Allows you to target the vcpu or
// vcpu group 'vid' if 'target' is one of the vcpu targets and 'self' has the
// necessary privileges to target a vcpu in another process.
extern errno_t Process_SendSignal(ProcessRef _Nonnull self, int target, pid_t id, vcpuid_t vid, int signo);

// Receives the signal 'signo' at the process 'self'. Returns EOK on success and
// EPERM if the sender 'sndr' does not have the permission to send 'signo' to
// the process 'self'.
extern errno_t Process_ReceiveSignal(ProcessRef _Nonnull self, const sig_sndr_t* _Nonnull sndr, int target, vcpuid_t vid, int signo);

// Same as Process_ReceiveSignal(), except that it assumes that 'signo' is sent
// internally from the process 'self' to the process 'self'. So it does not do
// any permission checking.
extern errno_t Process_ReceiveInternalSignal(ProcessRef _Nonnull self, int target, vcpuid_t vid, int signo);

// Adds or deletes a route for the signal 'signo'.
extern errno_t Process_Sigroute(ProcessRef _Nonnull self, int op, int signo, int target, id_t id);


// Sets/gets a scheduling parameter
extern errno_t Process_GetSchedParam(ProcessRef _Nonnull self, int type, int* _Nonnull param);
extern errno_t Process_SetSchedParam(ProcessRef _Nonnull self, int type, const int* _Nonnull param);


// Returns a copy of a specific process property.
extern errno_t Process_GetProperty(ProcessRef _Nonnull self, int flavor, char* _Nonnull buf, size_t bufSize);

// Returns a copy of the receiver's information.
extern errno_t Process_GetInfo(ProcessRef _Nonnull self, int flavor, proc_info_ref _Nonnull info);

// Returns an array of vcpuid_t's corresponding to the currently acquired vcpus.
extern errno_t Process_GetVirtualProcessorIds(ProcessRef _Nonnull self, vcpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore);

#endif /* Process_h */
