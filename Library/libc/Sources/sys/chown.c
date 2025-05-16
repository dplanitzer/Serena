//
//  chown.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


int chown(const char* _Nonnull path, uid_t uid, gid_t gid)
{
    return (int)_syscall(SC_chown, path, uid, gid);
}
