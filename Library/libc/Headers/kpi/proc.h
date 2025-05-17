//
//  kpi/proc.h
//  libc
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_PROC_H
#define _KPI_PROC_H 1

#include <kpi/kei.h>
#include <kpi/fcntl.h>
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
    kei_func_t* _Nonnull           urt_funcs;      // Pointer to the URT function table
} pargs_t;


// Process specific information
typedef struct procinfo {
    pid_t   pid;        // Process pid
    pid_t   ppid;       // Parent pid
    size_t  virt_size;  // Size of allocated address space
} procinfo_t;

// Returns general information about the process.
// get_procinfo(procinfo_t* _Nonnull pOutInfo)
#define kProcCommand_GetInfo    IOResourceCommand(0)

// Returns the name of the process.
// get_procname(char* _Nonnull buf, size_t bufSize)
#define kProcCommand_GetName    IOResourceCommand(1)

#endif /* _KPI_PROC_H */
