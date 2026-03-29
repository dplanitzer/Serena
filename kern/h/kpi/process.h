//
//  kpi/process.h
//  kpi
//
//  Created by Dietmar Planitzer on 2/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_PROCESS_H
#define _KPI_PROCESS_H 1

#include <_cmndef.h>
#include <kpi/kei.h>
#include <kpi/types.h>

#define PROC_SELF_ID    0

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


#define PROC_STATE_RUNNING_OLD  0
#define PROC_STATE_SLEEPING_OLD 1
#define PROC_STATE_STOPPED_OLD  2
#define PROC_STATE_EXITING_OLD  3
#define PROC_STATE_ZOMBIE_OLD   4

// Process specific information
typedef struct proc_info_old {
    pid_t   ppid;       // Parent pid
    pid_t   pid;        // Process pid
    pid_t   pgrp;       // Process group id
    pid_t   sid;        // Session id
    size_t  vcpu_count; // Number of vcpus currently assigned to the process
    size_t  virt_size;  // Size of allocated address space
    int     state;      // Current process state (PROC_STATE_XXX)
    uid_t   uid;        // User owning this process
} proc_info_old_t;

// Returns general information about the process.
// get_procinfo(proc_info_old_t* _Nonnull pOutInfo)
#define kProcCommand_GetInfo    IOResourceCommand(0)

// Returns the name of the process.
// get_procname(char* _Nonnull buf, size_t bufSize)
#define kProcCommand_GetName    IOResourceCommand(1)


#define JOIN_PROC       0   /* Join child process with pid */
#define JOIN_PROC_GROUP 1   /* Join any member of the child process group */
#define JOIN_ANY        2   /* Join any child process */


#define JREASON_EXIT        1
#define JREASON_SIGNAL      2
#define JREASON_EXCEPTION   3


// The result of a waitpid() system call.
struct proc_status {
    pid_t   pid;        // pid of the child process
    int     reason;     // termination reason
    union {
        int status;     // child process exit status
        int signo;      // signal that caused the process to terminate
        int excptno;    // exception that caused the process to terminate
    }       u;
};


// Tell umask() to just return the current umask without changing it
#define SEO_UMASK_NO_CHANGE -1


// Scheduling parameters that apply to a process as a whole
#define PROC_SCHED_QUANTUM_BOOST    1       /* param range: 0..15 */
#define PROC_SCHED_NICE             2       /* param range: 0..63 */


// Information about a process
typedef void* proc_info_ref;

#define PROC_INFO_IDS   1
#define PROC_INFO_CREDS 2


typedef struct proc_ids_info {
    pid_t   id;
    pid_t   parent_id;
    pid_t   group_id;
    pid_t   session_id;
} proc_ids_info_t;

typedef struct proc_creds_info {
    uid_t   uid;
    gid_t   gid;
} proc_creds_info_t;

#endif /* _KPI_PROCESS_H */
