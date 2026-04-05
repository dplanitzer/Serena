//
//  fs_sync.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


void fs_sync(void)
{
    (void)_syscall(SC_fs_sync);
}
