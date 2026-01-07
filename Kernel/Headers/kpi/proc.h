//
//  kpi/proc.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_PROC_H
#define _KPI_PROC_H 1

#include <_cmndef.h>
#include <kpi/kei.h>
#include <kpi/ioctl.h>
#include <kpi/stat.h>
#include <kpi/types.h>

// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
// This data structure is set up by the kernel when it processes a Spawn()
// request. Once set up the kernel neither reads nor writes to this area.
typedef struct pargs {
    size_t                      version;        // sizeof(pargs_t)
    size_t                      reserved;
    size_t                      arguments_size; // Size of the area that holds all of pargs_t + argv + envp
    size_t                      argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL
    char* _Nullable * _Nonnull  envp;           // Pointer to the base of the environment table. Last entry holds NULL.
    void* _Nonnull              image_base;     // Pointer to the base of the executable header
    kei_func_t* _Nonnull        urt_funcs;      // Pointer to the URT function table
} pargs_t;


#define PROC_STATE_RUNNING  0
#define PROC_STATE_SLEEPING 1
#define PROC_STATE_STOPPED  2
#define PROC_STATE_EXITING  3
#define PROC_STATE_ZOMBIE   4

// Process specific information
typedef struct proc_info {
    pid_t   ppid;       // Parent pid
    pid_t   pid;        // Process pid
    pid_t   pgrp;       // Process group id
    pid_t   sid;        // Session id
    size_t  vcpu_count; // Number of vcpus currently assigned to the process
    size_t  virt_size;  // Size of allocated address space
    int     state;      // Current process state (PROC_STATE_XXX)
    uid_t   uid;        // User owning this process
} proc_info_t;

// Returns general information about the process.
// get_procinfo(proc_info_t* _Nonnull pOutInfo)
#define kProcCommand_GetInfo    IOResourceCommand(0)

// Returns the name of the process.
// get_procname(char* _Nonnull buf, size_t bufSize)
#define kProcCommand_GetName    IOResourceCommand(1)

#endif /* _KPI_PROC_H */
