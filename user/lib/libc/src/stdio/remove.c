//
//  remove.c
//  libc
//
//  Created by Dietmar Planitzer on 8/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include <stdio.h>
#include <kpi/fcntl.h>
#include <kpi/syscall.h>


int remove(const char* _Nonnull path)
{
    return (int)_syscall(SC_unlink, path, __ULNK_ANY);
}
