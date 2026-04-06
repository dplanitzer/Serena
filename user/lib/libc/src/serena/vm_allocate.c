//
//  vm_allocate.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <serena/vm.h>
#include <kpi/syscall.h>


int vm_allocate(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return (int)_syscall(SC_vm_allocate, nbytes, ptr);
}
