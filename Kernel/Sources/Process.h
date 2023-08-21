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


extern ProcessRef _Nonnull  gRootProcess;


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

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously.
extern void Process_Terminate(ProcessRef _Nonnull pProc);

// Returns true if the process is marked for termination and false otherwise.
extern Bool Process_IsTerminating(ProcessRef _Nonnull pProc);

// Allocates more (user) address space to the given process.
extern ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, Int nbytes, Byte* _Nullable * _Nonnull pOutMem);

#endif /* Process_h */
