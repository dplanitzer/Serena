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
#include <ext/nanotime.h>
#include <kpi/kei.h>
#include <kpi/types.h>

#define PROC_SELF    0


// proc_ctx_t aux array entry types. Ignore types that you don't recognize. The
// array is terminated with a PROC_AUX_END entry.
#define PROC_AUX_END        0
#define PROC_AUX_EXEC_HDR   1   /* Pointer to the executable header */
#define PROC_AUX_KEI        2   /* Pointer to the KEI function table */

typedef struct proc_aux_entry {
    int     type;
    union {
        ssize_t i;
        void*   p;
    }       u;
} proc_aux_entry_t;

// The process context contains the argument and environment vectors of the
// process. It is set up at exec time and it is stored in the address space of
// the process. A pointer to this data structure is passed to the first function
// that is executed on the main vcpu. This function should store the pointer in
// a global variable to enable easy access to the reference data.
typedef struct proc_ctx {
    size_t                      argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL

    size_t                      envc;
    char* _Nullable * _Nonnull  envv;           // Pointer to the base of the environment table. Last entry holds NULL.

    proc_aux_entry_t* _Nonnull  aux;            // Pointer to an array of proc_aux_entry_t entries
} proc_ctx_t;


#define PROC_STOF_PID           0   /* Status of child process with pid */
#define PROC_STOF_ANY_FELLOW    1   /* Status of any member of the specified child process group */
#define PROC_STOF_ANY           2   /* Status of any of the process' child processes */

#define PROC_STF_NONBLOCKING    1    /* Do not block waiting for a status change. Return EAGAIN if no status change found.*/

#define PROC_STATUS_EXITED      1
#define PROC_STATUS_SIGNALED    2
#define PROC_STATUS_EXCEPTION   3


// Result of a proc_status() call.
typedef struct proc_status {
    pid_t   pid;        // pid of the child process
    int     reason;     // termination reason
    union {
        int status;     // child process exit status
        int signo;      // signal that caused the process to terminate
        int excptno;    // exception that caused the process to terminate
    }       u;
} proc_status_t;


// Scheduling parameters that apply to a process as a whole
#define PROC_SCHED_QUANTUM_BOOST    1       /* param range: 0..15 */
#define PROC_SCHED_NICE             2       /* param range: 0..63 */


// Information about a process
typedef void* proc_info_ref;

#define PROC_INFO_BASIC 1
#define PROC_INFO_TIMES 2


// Process run state
#define PROC_STATE_RESUMED      0       /* This means that either at least one vcpu is in running state or that all vcpus are waiting for something and the process is not suspended */
#define PROC_STATE_SUSPENDED    1       /* Process was explicitly suspended */
#define PROC_STATE_TERMINATING  2       /* Process has started executing an exit() call and is in the process of terminating */
#define PROC_STATE_TERMINATED   3       /* Process has terminated and parent processes hasn't reaped it yet */

// Process flags
#define PROC_FLAG_INCUBATING    1       /* Process is still in incubating state and will remain in this state until proc_exec() is called */


typedef struct proc_basic_info {
    pid_t           pid;
    pid_t           ppid;
    pid_t           pgrp;
    pid_t           sid;

    uid_t           uid;
    gid_t           gid;
    fs_perms_t      umask;

    int             run_state;
    unsigned int    flags;

    uint64_t        vm_size;                // size of process address space in bytes

    size_t          cmdline_size;           // size of the process command line string list including the terminating '\0'
    size_t          env_size;               // size of the process environment string list including the terminating '\0'

    size_t          vcpu_count;             // number of vcpus bound to process right now
    size_t          vcpu_lifetime_count;    // number of vcpus that have been bound to the process over its whole lifetime. Includes no longer acquired vcpus
    size_t          vcpu_waiting_count;     // number of vcpus that are currently bound to the process and in waiting or suspended state
} proc_basic_info_t;


typedef struct proc_times_info {
    nanotime_t  creation_time;      // Time when the process was created

    nanotime_t  user_time;          // Time the process has spent running in user mode since creation. This includes currently acquired and formerly acquired vcpus
    nanotime_t  system_time;        // Time the process has spent running in system/kernel mode since creation. This includes currently acquired and formerly acquired vcpus
    nanotime_t  wait_time;          // Time the process has spent in waiting or suspended state since creation. This includes currently acquired and formerly acquired vcpus

    nanotime_t  acq_user_time;      // Time the process has spent running in user mode since creation. Acquired vcpus only
    nanotime_t  acq_system_time;    // Time the process has spent running in system/kernel mode since creation. Acquired vcpus only
    nanotime_t  acq_wait_time;      // Time the process has spent in waiting or suspended state since creation. Acquired vcpus only
} proc_times_info_t;


// Process properties
#define PROC_PROP_CWD       1       /* current working directory of the process */
#define PROC_PROP_NAME      2       /* just the filename and extension portion of the process path */
#define PROC_PROP_PATH      3       /* path from which the process was executed */
#define PROC_PROP_CMDLINE   4       /* argv style, NUL separated command line arguments */
#define PROC_PROP_ENVIRON   5       /* argv style, NUL separated environment variable definitions */


// Well-known PIDs. These PIDs are guaranteed to remain stable and locked in
// from OS release to OS release 
#define PROC_PID_KERNELD    1
#define PROC_PID_SYSTEMD    2

#endif /* _KPI_PROCESS_H */
