//
//  getgid.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


gid_t getgid(void)
{
    return (gid_t)_syscall(SC_getgid);
}
