//
//  Process.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef Process_h
#define Process_h

#include <klib/klib.h>


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

// Triggers the termination of the given process. The termination may be caused
// voluntarily (some VP currently owned by the process triggers this call) or
// involuntarily (some other process triggers this call). Note that the actual
// termination is done asynchronously. 'exitCode' is the exit code that should
// be made available to the parent process. Note that the only exit code that
// is passed to the parent is the one from the first Process_Terminate() call.
// All others are discarded.
extern void Process_Terminate(ProcessRef _Nonnull pProc, Int exitCode);

// Returns true if the process is marked for termination and false otherwise.
extern Bool Process_IsTerminating(ProcessRef _Nonnull pProc);


extern Int Process_GetPID(ProcessRef _Nonnull pProc);

// Loads an executable from the given executable file into the process address
// space.
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be loacted at the address 'pExecAddr'
extern ErrorCode Process_Exec(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr);

extern ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure);

// Allocates more (user) address space to the given process.
extern ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ByteCount count, Byte* _Nullable * _Nonnull pOutMem);


// Adds the given process as a child to the given process. 'pOtherProc' must not
// already be a child of another process.
extern ErrorCode Process_AddChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);

// Removes the given process from 'pProc'. Does nothing if the given process is
// not a child of 'pProc'.
extern void Process_RemoveChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);

#endif /* Process_h */
