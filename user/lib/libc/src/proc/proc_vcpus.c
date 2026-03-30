//
//  proc_vcpus.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_vcpus(vcpuid_t* _Nonnull buf, size_t bufSize, vcpu_counts_t* _Nullable out_counts)
{
    return (int)_syscall(SC_proc_vcpus, buf, bufSize, out_counts);
}
