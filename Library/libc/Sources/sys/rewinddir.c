//
//  rewinddir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


errno_t rewinddir(int ioc)
{
    return (errno_t)_syscall(SC_lseek, ioc, (off_t)0ll, NULL, SEEK_SET);
}
