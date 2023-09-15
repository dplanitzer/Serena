//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Console.h"
#include "DriverManager.h"
#include "Process.h"
#include "VirtualProcessor.h"


typedef struct _SYS_read_args {
    Int                 scno;
    Character* _Nonnull buffer;
    ByteCount           count;
} SYS_read_args;

Int _SYSCALL_read(const SYS_read_args* _Nonnull pArgs)
{
    decl_try_err();
    Console* pConsole;

    try_null(pConsole, DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
    return Console_Read(pConsole, pArgs->buffer, pArgs->count);

catch:
    return -err;
}


typedef struct _SYS_write_args {
    Int                     scno;
    const Byte* _Nonnull    buffer;
    ByteCount               count;
} SYS_write_args;

Int _SYSCALL_write(const SYS_write_args* _Nonnull pArgs)
{
    decl_try_err();
    Console* pConsole;

    try_null(pConsole, DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
    return Console_Write(pConsole, pArgs->buffer, pArgs->count);

catch:
    return -err;
}


typedef struct _SYS_sleep_args {
    Int             scno;
    TimeInterval    ti;
} SYS_sleep_args;

Int _SYSCALL_sleep(const SYS_sleep_args* _Nonnull pArgs)
{
    return VirtualProcessor_Sleep(pArgs->ti);
}


typedef struct _SYS_dispatch_async_args {
    Int                             scno;
    const Closure1Arg_Func _Nonnull userClosure;
} SYS_dispatch_async_args;

Int _SYSCALL_dispatch_async(const SYS_dispatch_async_args* pArgs)
{
    return Process_DispatchAsyncUser(Process_GetCurrent(), pArgs->userClosure);
}


// Allocates more address space to the calling process. The address space is
// expanded by 'count' bytes. A pointer to the first byte in the newly allocated
// address space portion is return in 'pOutMem'. 'pOutMem' is set to NULL and a
// suitable error is returned if the allocation failed. 'count' must be greater
// than 0 and a multipler of the CPU page size.
typedef struct _SYS_alloc_address_space_args {
    Int                         scno;
    ByteCount                   count;
    Byte * _Nullable * _Nonnull pOutMem;
} SYS_alloc_address_space_args;

Int _SYSCALL_alloc_address_space(SYS_alloc_address_space_args* _Nonnull pArgs)
{
    return Process_AllocateAddressSpace(Process_GetCurrent(), pArgs->count, pArgs->pOutMem);
}


typedef struct _SYS_exit_args {
    Int scno;
    Int status;
} SYS_exit_args;

Int _SYSCALL_exit(const SYS_exit_args* _Nonnull pArgs)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate(Process_GetCurrent(), pArgs->status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(kTimeInterval_Infinity);
    return EOK;
}

typedef struct _SYS_spawn_process_args {
    Int                                             scno;
    Byte* _Nullable                                 execBase; //userEntryPoint;
    const Character* _Nullable const _Nullable *    argv;
    const Character* _Nullable const _Nullable *    envp;
} SYS_spawn_process_args;

Int _SYSCALL_spawn_process(const SYS_spawn_process_args* pArgs)
{
    decl_try_err();
    ProcessRef pCurProc = Process_GetCurrent();
    ProcessRef pChildProc = NULL;

    if (pArgs->execBase == NULL) {
        throw(EPARAM);
    }
    
    try(Process_Create(Process_GetNextAvailablePID(), &pChildProc));
    try_bang(Process_AddChildProcess(pCurProc, pChildProc));
    try(Process_Exec(pChildProc, pArgs->execBase, pArgs->argv, pArgs->envp));

    return EOK;

catch:
    if (pChildProc) {
        Process_RemoveChildProcess(pCurProc, pChildProc);
    }
    return err;
}
