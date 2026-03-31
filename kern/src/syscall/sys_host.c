//
//  sys_host.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <kpi/host.h>
#include <process/ProcessManager.h>



SYSCALL_3(host_procs, pid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    return ProcessManager_GetProcessIds(gProcessManager, pa->buf, pa->bufSize, pa->out_hasMore);
}
