//
//  VirtualProcessor_syscalls.c
//  Apollo
//
//  Created by Dietmar Planitzer on 7/19/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VirtualProcessor.h"
#include "VirtualProcessorScheduler.h"


// Exits the currently running virtual processor
ErrorCode _syscall_VirtualProcessor_Exit(void)
{
    VirtualProcessor_Exit(VirtualProcessor_GetCurrent());
    // NOT REACHED
    return EOK;
}

ErrorCode _syscall_VirtualProcessor_Sleep(__reg("d1") Int seconds, __reg("d2") Int nanoseconds)
{
    VirtualProcessor_Sleep(TimeInterval_Make(seconds, nanoseconds));
    return EOK;
}

ErrorCode _syscall_VirtualProcessor_Print(__reg("a1") const Character* str)
{
    print("%s", str);
    return EOK;
}
