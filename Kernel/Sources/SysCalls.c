//
//  Syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Process.h"
#include "VirtualProcessor.h"


ErrorCode _syscall_sleep(__reg("d1") Int seconds, __reg("d2") Int nanoseconds)
{
    VirtualProcessor_Sleep(TimeInterval_Make(seconds, nanoseconds));
    return EOK;
}

ErrorCode _syscall_dispatchAsync(__reg("a1") const Closure1Arg_Func pUserClosure)
{
    Process_DispatchAsyncUser(Process_GetCurrent(), pUserClosure);
    return EOK;
}

ErrorCode _syscall_print(__reg("a1") const Character* pString)
{
    print("%s", pString);
    return EOK;
}
