//
//  chdir.c
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>
#include <kern/_varargs.h>


int chdir(const char* _Nonnull path)
{
    return (int)_syscall(SC_chdir, path);
}
