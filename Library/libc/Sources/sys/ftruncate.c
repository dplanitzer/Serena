//
//  ftruncate.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <kpi/syscall.h>


int ftruncate(int fd, off_t length)
{
    return (int)_syscall(SC_ftruncate, fd, length);
}
