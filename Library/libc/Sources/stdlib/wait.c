//
//  wait.c
//  libc
//
//  Created by Dietmar Planitzer on 5/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/wait.h>
#include <kpi/syscall.h>


pid_t wait(int* _Nullable pstat)
{
    return waitpid((pid_t)-1, pstat, 0);
}
