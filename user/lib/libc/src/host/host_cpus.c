//
//  host_cpus.c
//  libc
//
//  Created by Dietmar Planitzer on 5/24/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/host.h>
#include <kpi/syscall.h>


int host_cpus(cpuid_t* _Nonnull buf, size_t bufSize)
{
    int hasMore = 0;
    
    if (_syscall(SC_host_cpus, buf, bufSize, &hasMore) == 0) {
        return hasMore;
    }
    else {
        return -1;
    }
}
