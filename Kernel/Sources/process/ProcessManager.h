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


// The process manager manages the set of processes that are alive and globally
// visible. Globally visible here means that it is possible to look up the
// processes by PID.
extern ProcessManagerRef _Nonnull  gProcessManager;


// Creates the process manager.
extern errno_t ProcessManager_Create(ProcessManagerRef _Nullable * _Nonnull pOutSelf);


// Returns the filesystem that represents the /proc catalog.
extern FilesystemRef _Nonnull ProcessManager_GetCatalog(ProcessManagerRef _Nonnull self);


// Publishes the given process and assigns it a unique pid.
// A process will only become visible to other processes after it has been
// published.
extern errno_t ProcessManager_Publish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp);

// Unpublishes the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// published.
extern void ProcessManager_Unpublish(ProcessManagerRef _Nonnull self, ProcessRef _Nonnull pp);


// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
extern ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull self, int pid);

// Returns the process 'pid' if it is a child of 'ppid' and it is in a zombie
// state.
extern ProcessRef _Nullable ProcessManager_CopyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pid, bool* _Nonnull pOutExists);

// Returns the first member of the process group 'pgrp' that is a child of the
// process 'ppid' and which is in zombie state.
extern ProcessRef _Nullable ProcessManager_CopyGroupZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, pid_t pgrp, bool* _Nonnull pOutAnyExists);

// Returns the first process that is a child of 'ppid' and which is in a
// zombie state.
extern ProcessRef _Nullable ProcessManager_CopyAnyZombieOfParent(ProcessManagerRef _Nonnull self, pid_t ppid, bool* _Nonnull pOutAnyExists);


// Send the signal 'signo' to one or multiple process(es) based on the given
// signalling scope.
extern errno_t ProcessManager_SendSignal(ProcessManagerRef _Nonnull self, const sigcred_t* _Nonnull sndr, int scope, id_t id, int signo);

#endif /* ProcessManager_h */
