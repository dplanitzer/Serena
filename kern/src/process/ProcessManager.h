//
//  ProcessManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ProcessManager_h
#define ProcessManager_h

#include "Process.h"
#include <kpi/host.h>


// The process manager manages the set of processes that are alive and globally
// visible. Globally visible here means that it is possible to look up the
// processes by PID.
extern ProcessManagerRef _Nonnull  gProcessManager;


// Creates the process manager.
extern errno_t ProcessManager_Create(ProcessManagerRef _Nullable * _Nonnull pOutSelf);


// Publishes the given process and assigns it a unique pid.
// A process will only become visible to other processes after it has been
// published. Returns ESRCH if 'pp' is a group member and no group leader or
// session leader exists for the provided group/session id. 
extern errno_t ProcessManager_Publish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp);

// Unpublishes the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// published.
extern void ProcessManager_Unpublish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp);


extern errno_t ProcessManager_GetStatusForProcessMatchingState(ProcessManagerRef _Nonnull self, int mstate, pid_t ppid, int match, pid_t id, proc_waitres_t* _Nonnull res);

// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
extern ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, int pid);

extern errno_t ProcessManager_GetProcessIds(ProcessManagerRef _Nonnull self, pid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore);


// Send the signal 'signo' to one or multiple process(es) based on the given
// signalling scope.
extern errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, int scope, id_t id, int signo);

#endif /* ProcessManager_h */
