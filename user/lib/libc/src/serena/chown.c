//
//  chown.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <kpi/syscall.h>
#include <serena/process.h>


int chown(const char* _Nonnull path, uid_t uid, gid_t gid)
{
    return (int)_syscall(SC_chown, path, uid, gid);
}
