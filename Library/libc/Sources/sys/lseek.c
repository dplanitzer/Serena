//
//  lseek.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <System/_syscall.h>
#include <System/_varargs.h>
#include <System/Error.h>


off_t lseek(int fd, off_t offset, int whence)
{
    off_t oldpos;
    const errno_t err = (errno_t)_syscall(SC_lseek, fd, offset, &oldpos, whence);

    if (err == EOK) {
        return oldpos;
    }
    else {
        return -1ll;
    }
}
