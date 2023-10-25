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

// The process spawn arguments specify how a child process should be created.
typedef struct __spawn_arguments_t SpawnArguments;


extern ProcessRef _Nonnull  gRootProcess;


// Returns the next PID available for use by a new process.
extern Int Process_GetNextAvailablePID(void);

// Returns the process associated with the calling execution context. Returns
// NULL if the execution context is not associated with a process. This will
// never be the case inside of a system call.
extern ProcessRef _Nullable Process_GetCurrent(void);


// Creates the root process which is the first process of the OS.
extern ErrorCode Process_CreateRootProcess(void* _Nonnull pExecBase, ProcessRef _Nullable * _Nonnull pOutProc);
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

// Spawns a new process that will be a child of the given process. The spawn
// arguments specify how the child process should be created, which arguments
// and environment it will receive and which descriptors it will inherit.
extern ErrorCode Process_SpawnChildProcess(ProcessRef _Nonnull pProc, const SpawnArguments* _Nonnull pArgs, ProcessRef _Nullable * _Nullable pOutChildProc);

extern ErrorCode Process_DispatchAsyncUser(ProcessRef _Nonnull pProc, Closure1Arg_Func pUserClosure);

// Allocates more (user) address space to the given process.
extern ErrorCode Process_AllocateAddressSpace(ProcessRef _Nonnull pProc, ByteCount count, Byte* _Nullable * _Nonnull pOutMem);


// Registers the given user object with the process. This action allows the
// process to use this user object. The process maintains a strong reference to
// the object until it is unregistered. Note that the process retains the object
// and thus you have to release it once the call returns. The call returns a
// descriptor which can be used to refer to the object from user and/or kernel
// space.
extern ErrorCode Process_RegisterUObject(ProcessRef _Nonnull pProc, ObjectRef _Nonnull pObject, Int* _Nonnull pOutDescriptor);

// Unregisters the user object identified by the given descriptor. The object is
// removed from the process' user object table and a strong reference to the
// object is returned. The caller should call close() on the object to close it
// and then release() to release the strong reference to the object. Closing the
// object will mark the object as done and the object will be deallocated once
// the last strong reference to it has been released.
extern ErrorCode Process_UnregisterUObject(ProcessRef _Nonnull pProc, Int fd, ObjectRef _Nullable * _Nonnull pOutObject);

// Looks up the user object identified by the given descriptor and returns a
// strong reference to it if found. The caller should call release() on the
// object once it is no longer needed.
extern ErrorCode Process_GetOwnedUObjectForDescriptor(ProcessRef _Nonnull pProc, Int fd, ObjectRef _Nullable * _Nonnull pOutObject);

#endif /* Process_h */
