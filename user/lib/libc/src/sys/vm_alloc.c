//
//  vm_alloc.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vm.h>
#include <kpi/syscall.h>


int vm_alloc(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return (int)_syscall(SC_vmalloc, nbytes, ptr);
}
