//
//  proc_vcpus.c
//  libc
//
//  Created by Dietmar Planitzer on 3/28/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <serena/process.h>
#include <kpi/syscall.h>


int proc_vcpus(const vcpu_matcher_t* _Nullable matchers, vcpuid_t* _Nonnull buf, size_t bufSize)
{
    int hasMore = 0;
    
    if (_syscall(SC_proc_vcpus, matchers, buf, bufSize, &hasMore) == 0) {
        return hasMore;
    }
    else {
        return -1;
    }
}
