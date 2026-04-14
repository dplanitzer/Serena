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

#define PROC_SELF    0

// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
// This data structure is set up by the kernel when it processes a Spawn()
// request. Once set up the kernel neither reads nor writes to this area.
typedef struct proc_args {
    size_t                      version;        // sizeof(proc_args_t)
    size_t                      reserved;
    size_t                      pargs_size;     // Size of the area that holds all of proc_args_t + argv + envp
    size_t                      argv_size;
    size_t                      env_size;
    size_t                      argc;           // Number of command line arguments passed to the process. Argv[0] holds the path to the process through which it was started
    char* _Nullable * _Nonnull  argv;           // Pointer to the base of the command line arguments table. Last entry is NULL
    char* _Nullable * _Nonnull  envp;           // Pointer to the base of the environment table. Last entry holds NULL.
    void* _Nonnull              image_base;     // Pointer to the base of the executable header
    kei_func_t* _Nonnull        kei_funcs;      // Pointer to the URT function table
} proc_args_t;


#define PROC_STOF_PID           0   /* Status of child process with pid */
#define PROC_STOF_ANY_FELLOW    1   /* Status of any member of the specified child process group */
#define PROC_STOF_ANY           2   /* Status of any of the process' child processes */

#define PROC_STF_NONBLOCKING   1    /* Do not block waiting for a status change. Return EAGAIN if no status change found.*/

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
#define PROC_INFO_IDS   2
#define PROC_INFO_USER  3
#define PROC_INFO_TIMES 4


#define PROC_STATE_RESUMED      0       /* This means that either at least one vcpu is in running state or that all vcpus are waiting for something and the process is not suspended */
#define PROC_STATE_SUSPENDED    1       /* Process was explicitly suspended */
#define PROC_STATE_TERMINATING  2       /* Process has started executing an exit() call and is in the process of terminating */
#define PROC_STATE_TERMINATED   3       /* Process has terminated and parent processes hasn't reaped it yet */

typedef struct proc_basic_info {
    int run_state;

    size_t  vcpu_count;             // number of vcpus bound to process right now
    size_t  vcpu_lifetime_count;    // number of vcpus that have been bound to the process over its whole lifetime. Includes no longer acquired vcpus
    size_t  vcpu_waiting_count;     // number of vcpus that are currently bound to the process and in waiting or suspended state

    size_t  vm_size;                // size of process address space in bytes

    size_t  argv_size;              // size of the argv vector
    size_t  env_size;               // size of the environment variable table
} proc_basic_info_t;


typedef struct proc_ids_info {
    pid_t   id;
    pid_t   parent_id;
    pid_t   group_id;
    pid_t   session_id;
} proc_ids_info_t;


typedef struct proc_user_info {
    uid_t       uid;
    gid_t       gid;
    fs_perms_t  umask;
} proc_user_info_t;


typedef struct proc_times_info {
    struct timespec creation_time;      // Time when the process was created

    struct timespec user_time;          // Time the process has spent running in user mode since creation. This includes currently acquired and formerly acquired vcpus
    struct timespec system_time;        // Time the process has spent running in system/kernel mode since creation. This includes currently acquired and formerly acquired vcpus
    struct timespec wait_time;          // Time the process has spent in waiting or suspended state since creation. This includes currently acquired and formerly acquired vcpus

    struct timespec acq_user_time;      // Time the process has spent running in user mode since creation. Acquired vcpus only
    struct timespec acq_system_time;    // Time the process has spent running in system/kernel mode since creation. Acquired vcpus only
    struct timespec acq_wait_time;      // Time the process has spent in waiting or suspended state since creation. Acquired vcpus only
} proc_times_info_t;


// Process properties
#define PROC_PROP_CWD       1
#define PROC_PROP_NAME      2
#define PROC_PROP_PATH      3
#define PROC_PROP_ARGS      4
#define PROC_PROP_ENVIRON   5

#endif /* _KPI_PROCESS_H */
