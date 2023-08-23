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


ErrorCode _SYSCALL_write(const Character* pString)
{
    decl_try_err();
    Console* pConsole;

    try_null(pConsole, DriverManager_GetDriverForName(gDriverManager, kConsoleName), ENODEV);
    Console_DrawString(pConsole, pString);
    return EOK;

catch:
    return err;
}

ErrorCode _SYSCALL_sleep(Int seconds, Int nanoseconds)
{
    return VirtualProcessor_Sleep(TimeInterval_Make(seconds, nanoseconds));
}

ErrorCode _SYSCALL_dispatch_async(const Closure1Arg_Func pUserClosure)
{
    return Process_DispatchAsyncUser(Process_GetCurrent(), pUserClosure);
}

ErrorCode _SYSCALL_alloc_address_space(Int nbytes)
{
    return Process_AllocateAddressSpace(Process_GetCurrent(), nbytes, (Byte**)&VirtualProcessor_GetCurrent()->syscall_ret_0);
}

ErrorCode _SYSCALL_exit(Int status)
{
    // Trigger the termination of the process. Note that the actual termination
    // is done asynchronously. That's why we sleep below since we don't want to
    // return to user space anymore.
    Process_Terminate(Process_GetCurrent(), status);


    // This wait here will eventually be aborted when the dispatch queue that
    // owns this VP is terminated. This interrupt will be caused by the abort
    // of the call-as-user and thus this system call will not return to user
    // space anymore. Instead it will return to the dispatch queue main loop.
    VirtualProcessor_Sleep(kTimeInterval_Infinity);
    return EOK;
}

ErrorCode _SYSCALL_spawn_process(Byte* _Nullable pUserEntryPoint)
{
    decl_try_err();
    ProcessRef pCurProc = Process_GetCurrent();
    ProcessRef pChildProc = NULL;

    if (pUserEntryPoint == NULL) {
        throw(EPARAM);
    }
    
    try(Process_Create(Process_GetNextAvailablePID(), &pChildProc));
    try_bang(Process_AddChildProcess(pCurProc, pChildProc));
    try(Process_DispatchAsyncUser(pChildProc, (Closure1Arg_Func)pUserEntryPoint));

    return EOK;

catch:
    if (pChildProc) {
        Process_RemoveChildProcess(pCurProc, pChildProc);
    }
    return err;
}
