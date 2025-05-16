//
//  umask.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/proc.h>
#include <sys/_syscall.h>
#include <System/_varargs.h>


FilePermissions getumask(void)
{
    return _syscall(SC_getumask);
}

void setumask(FilePermissions mask)
{
    _syscall(SC_setumask, mask);
}
