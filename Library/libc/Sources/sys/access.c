//
//  access.c
//  libc
//
//  Created by Dietmar Planitzer on 5/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <unistd.h>
#include <sys/_syscall.h>


int access(const char* _Nonnull path, int mode)
{
    return (int)_syscall(SC_access, path, mode);
}
