//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Console.h"
#include "Process.h"
#include "SystemGlobals.h"
#include "VirtualProcessor.h"


ErrorCode _syscall_write(const Character* pString)
{
    return Console_DrawString(gConsole, pString);
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
