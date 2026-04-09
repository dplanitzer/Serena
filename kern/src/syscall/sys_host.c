//
//  sys_host.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <hal/sys_desc.h>
#include <kpi/host.h>
#include <filemanager/FilesystemManager.h>
#include <process/ProcessManager.h>


SYSCALL_2(host_info, int flavor, host_info_ref _Nonnull info)
{
    decl_try_err();

    switch (pa->flavor) {
        case HOST_INFO_BASIC: {
            host_basic_info_t* ip = pa->info;

            ip->cpu_type = g_sys_desc->cpu_type;
            ip->cpu_subtype = g_sys_desc->cpu_subtype;
            ip->physical_cpu_count = 1;
            ip->physical_max_cpu_count = 1;
            ip->logical_cpu_count = 1;
            ip->logical_max_cpu_count = 1;
            ip->phys_mem_size = sys_desc_getramsize(g_sys_desc);
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    return err;
}

SYSCALL_3(host_processes, pid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    return ProcessManager_GetProcessIds(gProcessManager, pa->buf, pa->bufSize, pa->out_hasMore);
}

SYSCALL_3(host_filesystems, fsid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    return ProcessManager_GetProcessIds(gProcessManager, pa->buf, pa->bufSize, pa->out_hasMore);
}
