//
//  sys_host.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "syscalldecls.h"
#include <hal/clock.h>
#include <hal/sys_desc.h>
#include <kpi/host.h>
#include <filemanager/FilesystemManager.h>
#include <process/ProcessManager.h>
#include <sched/vcpu_pool.h>


SYSCALL_2(host_info, int flavor, host_info_ref _Nonnull info)
{
    decl_try_err();

    switch (pa->flavor) {
        case HOST_INFO_BASIC: {
            host_basic_info_t* ip = pa->info;

            ip->cpu_type = g_sys_desc->cpu_type;
            ip->cpu_subtype = g_sys_desc->cpu_subtype;
            ip->physical_cpu_count = 1;     //XXX hardcoded to a single CPU for now
            ip->physical_max_cpu_count = 1;
            ip->logical_cpu_count = 1;
            ip->logical_max_cpu_count = 1;
            ip->phys_mem_size = sys_desc_getramsize(g_sys_desc);
            ip->page_size = CPU_PAGE_SIZE;
            break;
        }

        case HOST_INFO_RESCOUNTS: {
            host_rescounts_info_t* ip = pa->info;

            ip->proc_count = ProcessManager_GetProcessCount(gProcessManager);
            ip->vcpu_pool_size = vcpu_pool_size(g_vcpu_pool);
            ip->fs_count = FilesystemManager_GetFilesystemCount(gFilesystemManager);
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

//XXX hardcoded to a single CPU for now
SYSCALL_3(host_cpus, cpuid_t* _Nonnull buf, size_t bufSize, int* _Nonnull out_hasMore)
{
    // Min is 2 because there must be one process that is executing this code + the trailing 0
    if (pa->bufSize < 2) {
        return ERANGE;
    }

    pa->buf[0] = 1;
    pa->buf[1] = 0;

    *(pa->out_hasMore) = 0;
    return EOK;
}

//XXX hardcoded to a single CPU for now
SYSCALL_3(cpu_info, cpuid_t cpuid, int flavor, cpu_info_ref _Nonnull info)
{
    decl_try_err();

    switch (pa->flavor) {
        case CPU_INFO_BASIC: {
            cpu_basic_info_t* ip = pa->info;

            ip->cpu_type = g_sys_desc->cpu_type;
            ip->cpu_subtype = g_sys_desc->cpu_subtype;
            ip->physical_cpuid = 1;
            ip->pkgid = 1;
            break;
        }

        case CPU_INFO_UTILIZATION: {
            cpu_utilization_info_t* ip = pa->info;

            clock_ticks2time(g_mono_clock, g_sched->usr_ticks, &ip->user_time);
            clock_ticks2time(g_mono_clock, g_sched->sys_ticks, &ip->system_time);
            clock_ticks2time(g_mono_clock, g_sched->idle_ticks, &ip->idle_time);
            break;
        }

        default:
            err = EINVAL;
            break;
    }

    return err;
}
