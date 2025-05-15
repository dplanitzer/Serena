//
//  ftruncate.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <System/_syscall.h>
#include <System/_varargs.h>


int ftruncate(int fd, off_t length)
{
    return (int)_syscall(SC_ftruncate, fd, length);
}
