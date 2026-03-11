//
//  getpargs.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/proc.h>
#include <kpi/syscall.h>


pargs_t* _Nonnull getpargs(void)
{
    return (pargs_t*) _syscall(SC_getpargs);
}
