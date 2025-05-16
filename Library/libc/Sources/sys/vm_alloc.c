//
//  vm_alloc.c
//  libc
//
//  Created by Dietmar Planitzer on 5/15/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <sys/vm.h>
#include <sys/_syscall.h>


int vm_alloc(size_t nbytes, void* _Nullable * _Nonnull ptr)
{
    return _syscall(SC_vmalloc, nbytes, ptr);
}
