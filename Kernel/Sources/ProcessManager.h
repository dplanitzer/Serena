//
//  ProcessManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/28/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ProcessManager_h
#define ProcessManager_h

#include <klib/klib.h>
#include "Process.h"

struct _ProcessManager;
typedef struct _ProcessManager* ProcessManagerRef;


// The process manager manages the set of processes that are alive and globally
// visible. Globally visible here means that it is possible to look up the
// processes by PID.
extern ProcessManagerRef _Nonnull  gProcessManager;


// Creates the process manager. The provided process becomes the root process.
extern errno_t ProcessManager_Create(ProcessRef _Nonnull pRootProc, ProcessManagerRef _Nullable * _Nonnull pOutManager);

// Returns a strong reference to the root process. This is the process that has
// no parent but all other processes are directly or indirectly descendants of
// the root process. The root process never changes identity and never goes
// away.
extern ProcessRef _Nonnull ProcessManager_CopyRootProcess(ProcessManagerRef _Nonnull pManager);

// Looks up the process for the given PID. Returns NULL if no such process is
// registered with the process manager and otherwise returns a strong reference
// to the process object. The caller is responsible for releasing the reference
// once no longer needed.
extern ProcessRef _Nullable ProcessManager_CopyProcessForPid(ProcessManagerRef _Nonnull pManager, int pid);


// Registers the given process with the process manager. Note that this function
// does not validate whether the process is already registered or has a PID
// that's equal to some other registered process.
// A process will only become visible to other processes after it has been
// registered with the process manager.
extern errno_t ProcessManager_Register(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc);

// Deregisters the given process from the process manager. This makes the process
// invisible to other processes. Does nothing if the given process isn't
// registered.
extern void ProcessManager_Unregister(ProcessManagerRef _Nonnull pManager, ProcessRef _Nonnull pProc);

#endif /* ProcessManager_h */
