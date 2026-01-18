//
//  sys_vm.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/5/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <ext/limits.h>
#include <vm/AddressSpace.h>


SYSCALL_2(alloc_address_space, size_t nbytes, void * _Nullable * _Nonnull pOutMem)
{
    if (pa->nbytes > SSIZE_MAX) {
        return E2BIG;
    }

    return AddressSpace_Allocate(&(vp->proc->addr_space),
            __SSizeByClampingSize(pa->nbytes),
            pa->pOutMem);
}
