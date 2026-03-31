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
#include <ext/timespec.h>
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

#define PROC_INFO_BASIC 1
#define PROC_INFO_IDS   2
#define PROC_INFO_CREDS 3
#define PROC_INFO_TIMES 4


#define PROC_STATE_ALIVE    0       /* This means that either at least one vcpu is in running state or that all vcpus are waiting for something */
#define PROC_STATE_STOPPED  1       /* Process is explicitly suspended */
#define PROC_STATE_EXITING  2       /* Process has received an exit() call and is in the process of terminating */
#define PROC_STATE_ZOMBIE   3       /* Process has terminated and parent processes hasn't reaped it yet */

typedef struct proc_basic_info {
    int run_state;

    size_t  vcpu_count;             // number of vcpus bound to process right now
    size_t  vcpu_lifetime_count;    // number of vcpus that have been bound to the process over its whole lifetime. Includes no longer acquired vcpus
    size_t  vcpu_waiting_count;     // number of vcpus that are currently bound to the process and in waiting or suspended state

    size_t  vm_size;                // size of process address space in bytes
} proc_basic_info_t;


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


typedef struct proc_times_info {
    struct timespec creation_time;      // Time when the process was created

    struct timespec user_time;          // Time the process has spent running in user mode since creation. This includes currently acquired and formerly acquired vcpus
    struct timespec system_time;        // Time the process has spent running in system/kernel mode since creation. This includes currently acquired and formerly acquired vcpus
    struct timespec wait_time;          // Time the process has spent in waiting or suspended state since creation. This includes currently acquired and formerly acquired vcpus

    struct timespec acq_user_time;      // Time the process has spent running in user mode since creation. Acquired vcpus only
    struct timespec acq_system_time;    // Time the process has spent running in system/kernel mode since creation. Acquired vcpus only
    struct timespec acq_wait_time;      // Time the process has spent in waiting or suspended state since creation. Acquired vcpus only
} proc_times_info_t;

#endif /* _KPI_PROCESS_H */
