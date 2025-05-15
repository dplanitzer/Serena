//
//  __vcpuerrno.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <System/Error.h>
#include <System/_syscall.h>


errno_t* _Nonnull __vcpuerrno(void)
{
    return (errno_t*)_syscall(SC_vcpuerrno);
}
