//
//  __vcpuerrno.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/errno.h>
#include <sys/_syscall.h>


errno_t* _Nonnull __vcpuerrno(void)
{
    return (errno_t*)_syscall(SC_vcpuerrno);
}
