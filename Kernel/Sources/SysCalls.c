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


ErrorCode _syscall_write(const Character* pString)
{
    decl_try_err();
    Console* pConsole = NULL;
    
    try(DriverManager_GetDriverForName(gDriverManager, kConsoleName, (DriverRef*)&pConsole));
    try(Console_DrawString(pConsole, pString));
    return EOK;

catch:
    return err;
}

ErrorCode _syscall_sleep(Int seconds, Int nanoseconds)
{
    return VirtualProcessor_Sleep(TimeInterval_Make(seconds, nanoseconds));
}

ErrorCode _syscall_dispatchAsync(const Closure1Arg_Func pUserClosure)
{
    return Process_DispatchAsyncUser(Process_GetCurrent(), pUserClosure);
}

ErrorCode _syscall_exit(Int status)
{
    Process_Exit(Process_GetCurrent());
    return EOK;
}
