//
//  host_procs.c
//  libc
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/host.h>
#include <kpi/syscall.h>


int host_procs(const proc_matcher_t* _Nullable matchers, pid_t* _Nonnull buf, size_t bufSize)
{
    int hasMore = 0;
    
    if (_syscall(SC_proc_vcpus, matchers, buf, bufSize, &hasMore) == 0) {
        return hasMore;
    }
    else {
        return -1;
    }
}
