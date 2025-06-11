//
//  waitpid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/wait.h>
#include <kpi/syscall.h>


pid_t waitpid(pid_t pid, int* _Nullable pstat, int options)
{
    struct _pstatus s;

    if (_syscall(SC_waitpid, pid, &s, options) == 0) {
        if (pstat) {
            *pstat = s.status;
        }
        return s.pid;
    }
    else {
        return -1;
    }
}
