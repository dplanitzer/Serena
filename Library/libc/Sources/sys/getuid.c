//
//  getuid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <kpi/syscall.h>


uid_t getuid(void)
{
    return (uid_t)_syscall(SC_getuid);
}
