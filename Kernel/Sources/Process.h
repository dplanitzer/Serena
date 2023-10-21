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
#include "Object.h"


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


extern Int Process_GetId(ProcessRef _Nonnull pProc);
extern Int Process_GetParentId(ProcessRef _Nonnull pProc);

// Returns the base address of the process arguments area. The address is
// relative to the process address space.
extern void* Process_GetArgumentsBaseAddress(ProcessRef _Nonnull pProc);

// Loads an executable from the given executable file into the process address
// space.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// \param pArgv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param pEnv the environment for teh process. Null means that the process inherits the environment from its parent
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be loacted at the address 'pExecAddr'
extern ErrorCode Process_Exec(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr, const Character* const _Nullable * _Nullable pArgv, const Character* const _Nullable * _Nullable pEnv);

extern ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure);

// Allocates more (user) address space to the given process.
extern ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ByteCount count, Byte* _Nullable * _Nonnull pOutMem);


// Adds the given process as a child to the given process. 'pOtherProc' must not
// already be a child of another process.
extern ErrorCode Process_AddChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);

// Removes the given process from 'pProc'. Does nothing if the given process is
// not a child of 'pProc'.
extern void Process_RemoveChildProcess(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);


// Registers the given user object with the process. This action allows the
// process to use this user object. The process maintains a strong reference to
// the object until it is unregistered. Note that the process retains the object
// and thus you have to release it once the call returns. The call returns a
// descriptor which can be used to refer to the object from user and/or kernel
// space.
extern ErrorCode Process_RegisterUObject(ProcessRef _Nonnull pProc, UObjectRef _Nonnull pObject, Int* _Nonnull pOutDescriptor);

// Unregisters the user object identified by the given descriptor. The object is
// removed from the process' user object table and a strong reference to the
// object is returned. The caller should call close() on the object to close it
// and then release() to release the strong reference to the object. Closing the
// object will mark the object as done and the object will be deallocated once
// the last strong reference to it has been released.
extern ErrorCode Process_UnregisterUObject(ProcessRef _Nonnull pProc, Int fd, UObjectRef _Nullable * _Nonnull pOutObject);

// Looks up the user object identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// object once it is no longer needed.
extern ErrorCode Process_GetOwnedUObjectForDescriptor(ProcessRef _Nonnull pProc, Int fd, UObjectRef _Nullable * _Nonnull pOutObject);

#endif /* Process_h */
