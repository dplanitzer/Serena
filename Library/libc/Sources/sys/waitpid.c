//
//  waitpid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/wait.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


errno_t waitpid(pid_t pid, pstatus_t* _Nullable result)
{
    return _syscall(SC_waitpid, pid, result);
}
