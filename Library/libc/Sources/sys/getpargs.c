//
//  getpargs.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/proc.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


pargs_t* _Nonnull getpargs(void)
{
    return (pargs_t*) _syscall(SC_getpargs);
}
