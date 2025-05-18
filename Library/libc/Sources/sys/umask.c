//
//  umask.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/proc.h>
#include <sys/_syscall.h>


mode_t umask(mode_t mask)
{
    return _syscall(SC_umask, mask);
}
