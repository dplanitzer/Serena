//
//  closedir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <dirent.h>
#include <sys/_syscall.h>


int closedir(DIR* _Nullable dir)
{
    if (dir) {
        return (int)_syscall(SC_close, (int)dir - __DIR_BASE);
    }
    else {
        return 0;
    }
}
