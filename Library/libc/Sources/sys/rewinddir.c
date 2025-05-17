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


errno_t rewinddir(DIR* _Nonnull dir)
{
    return (errno_t)_syscall(SC_lseek, (int)dir - __DIR_BASE, (off_t)0ll, NULL, SEEK_SET);
}
