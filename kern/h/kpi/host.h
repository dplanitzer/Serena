//
//  kpi/host.h
//  kpi
//
//  Created by Dietmar Planitzer on 3/30/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HOST_H
#define _KPI_HOST_H 1

#include <_cmndef.h>
#include <ext/nanotime.h>
#include <kpi/types.h>
#include <machine/cpu.h>

// Information about a host
typedef void* host_info_ref;

#define HOST_INFO_BASIC     1
#define HOST_INFO_RESCOUNTS 2


typedef struct host_basic_info {
    cpu_type_t      cpu_type;
    cpu_subtype_t   cpu_subtype;
    
    size_t          physical_cpu_count;		// number of physical cpus available now
    size_t          physical_max_cpu_count;	// max number of phys cpus available
    size_t          logical_cpu_count;		// number of log cpus available now
    size_t          logical_max_cpu_count;	// max number of log cpus available

    uint64_t        phys_mem_size;
    size_t          page_size;
} host_basic_info_t;


// Use this to get a pretty good estimate of how big the buffer needs to be that
// you should pass to host_processes(), host_filesystems(), etc.
typedef struct host_rescounts_info {
    size_t  proc_count;     // number of process alive right now
    size_t  vcpu_pool_size; // number of vcpus currently in the global vcpu pool
    size_t  fs_count;       // number of filesystems registered right now
} host_rescounts_info_t;

//XXX NOT YET
//		-- sched info
//          --- scheduler clock id
//			--- min timeout ??? may not be needed since can use clock_info(); though more convenient to have it here
//			--- min quantum size
//		--- load info
//			--- loadavg[3]
//      -- kernel info
//          -- semantic version number
//XXX
#endif /* _KPI_HOST_H */
