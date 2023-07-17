//
//  Process.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include "Foundation.h"


struct _Process;
typedef struct _Process* ProcessRef;


// Returns the next PID available for use by a new process.
extern Int Process_GetNextAvailablePID(void);

// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
extern ProcessRef _Nullable Process_GetCurrent(void);


extern ErrorCode Process_Create(Int pid, ProcessRef _Nullable * _Nonnull pOutProc);
extern void Process_Destroy(ProcessRef _Nullable pProc);

extern Int Process_GetPID(ProcessRef _Nonnull pProc);
extern ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure);

// Voluntarily exits the given process. Assumes that this function is called from
// a system call originating in user space and that this call is made from one of
// the dispatch queue execution contexts owned by the process.
extern void Process_Exit(ProcessRef _Nonnull pProc);

#endif /* Process_h */
