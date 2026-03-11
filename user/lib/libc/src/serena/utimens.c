//
//  utimens.c
//  libc
//
//  Created by Dietmar Planitzer on 5/14/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/file.h>
#include <kpi/syscall.h>


int utimens(const char* _Nonnull path, const struct timespec times[_Nullable 2])
{
    return (int)_syscall(SC_utimens, path, times);
}
