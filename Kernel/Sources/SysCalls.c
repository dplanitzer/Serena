//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Process.h"
#include "VirtualProcessor.h"


ErrorCode _syscall_write(const Character* pString)
{
    print("%s", pString);
    return EOK;
}

ErrorCode _syscall_sleep(Int seconds, Int nanoseconds)
{
    VirtualProcessor_Sleep(TimeInterval_Make(seconds, nanoseconds));
    return EOK;
}

ErrorCode _syscall_dispatchAsync(const Closure1Arg_Func pUserClosure)
{
    Process_DispatchAsyncUser(Process_GetCurrent(), pUserClosure);
    return EOK;
}

ErrorCode _syscall_exit(Int status)
{
    Process_Exit(Process_GetCurrent());
    return EOK;
}
