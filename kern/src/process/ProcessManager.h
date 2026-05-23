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


// Registers the given process and assigns it a unique pid.
// A process will only become visible to other processes after it has been
// registered. Returns ESRCH if 'pp' is a group member and no group leader or
// session leader exists for the provided group/session id. 
extern errno_t ProcessManager_Register(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp);


// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
extern ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, int pid);

// Returns the number of processes currently registered.
extern size_t ProcessManager_GetProcessCount(ProcessManagerRef _Nonnull self);

extern errno_t ProcessManager_GetProcessIds(ProcessManagerRef _Nonnull self, pid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore);

extern errno_t ProcessManager_GetStatusForProcessMatchingState(ProcessManagerRef _Nonnull self, int mstate, pid_t ppid, int match, pid_t id, proc_waitres_t* _Nonnull res);


// Send the signal 'signo' to one or multiple process(es).
extern errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, const sig_dispatch_t* _Nonnull dp);

#endif /* ProcessManager_h */
