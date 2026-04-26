//
//  ProcessPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ProcessPriv_h
#define ProcessPriv_h

#include "Process.h"
#include "IOChannelTable.h"
#include <ext/rc.h>
#include <filemanager/FileManager.h>
#include <kpi/signal.h>
#include <sched/mtx.h>
#include <sched/waitqueue.h>
#include <vm/AddressSpace.h>


// Default stack size for user space vcpus
#define PROC_DEFAULT_USER_STACK_SIZE    CPU_PAGE_SIZE


// User space wait queues
#define UWQ_HASH_CHAIN_COUNT    4
#define UWQ_HASH_CHAIN_MASK     (UWQ_HASH_CHAIN_COUNT - 1)

struct u_wait_queue {
    deque_node_t        qe;
    struct waitqueue    wq;
    unsigned int        policy;
    int                 id;
};
typedef struct u_wait_queue* u_wait_queue_t;


// Signal routes
struct sigroute {
    queue_node_t    qe;
    id_t            target_id;
    int16_t         use_count;
    int8_t          signo;
    int8_t          scope;
};
typedef struct sigroute* sigroute_t;


// Process relationship information (owned & protected by ProcessManager)
typedef struct proc_rel {
    queue_node_t            pid_qe;     // pid_table chain entry.
    queue_t/*<Process>*/    children;
    queue_node_t            child_qe;
} proc_rel_t;



typedef struct Process {
    proc_rel_t                      rel;        // process relationships maintained by the process manager. Must be the first field here

    ref_count_t                     retainCount;
    mtx_t                           mtx;

    pid_t                           pid;        // my PID
    pid_t                           ppid;       // parent's PID
    pid_t                           pgrp;       // Group id. I'm the group leader if pgrp == pid
    pid_t                           sid;        // (Login) session id. I'm the session leader if sid == pid 

    // Process run state and flags
    int                             run_state;
    unsigned int                    flags;

    // Process image
    AddressSpace                    addr_space;
    proc_ctx_t* _Nullable           ctx_base;       // Base address to the contiguous memory region holding the proc_ctx_t structure, command line arguments and environment
    size_t                          arg_size;       // Size of arg_strings in terms of bytes. Includes the trailing '\0'
    char* _Nullable                 arg_strings;    // Consecutive list of NUL-terminated process argument strings. End is marked by an empty string  
    size_t                          env_size;       // Size of env_strings in terms of bytes. Includes the trailing '\0'
    char* _Nullable                 env_strings;    // Consecutive list of NUL-terminated process environment strings. End is marked by an empty string

    excpt_handler_t                 excpt_handler;  // atomic: exception handler and argument

    // VPs
    deque_t                         vcpu_queue;     // deque_t of VPs [mtx]
    size_t                          vcpu_count;
    size_t                          vcpu_lifetime_count;
    size_t                          vcpu_waiting_count;
    vcpuid_t                        next_avail_vcpuid;

    // Scheduling parameters
    int8_t                          quantum_boost;
    int8_t                          sched_nice;
    int8_t                          reserved[2];

    // I/O Channels
    IOChannelTable                  ioChannelTable;     // I/O channels (aka sharable resources)
    
    // File manager
    FileManager                     fm;

    // Times stats
    nanotime_t                      creation_time;
    ticks_t                         user_ticks;         // number of clock ticks this process has spent running in user space across all vcpus (former + current)
    ticks_t                         system_ticks;       // number of clock ticks this process has spent running in system space across all vcpus (former + current)
    ticks_t                         wait_ticks;         // number of clock ticks this process has spent waiting or suspended across all vcpus (former + current)
    ticks_t                         rq_user_ticks;      // number of clock ticks this process has spent running in user space across all relinquished vcpus
    ticks_t                         rq_system_ticks;    // number of clock ticks this process has spent running in system space across all relinquished vcpus
    ticks_t                         rq_wait_ticks;      // number of clock ticks this process has spent waiting or suspended across all relinquished vcpus

    // User wait queues
    deque_t/*<struct u_wait_queue>*/waitQueueTable[UWQ_HASH_CHAIN_COUNT];   // wait queue descriptor -> struct u_wait_queue
    int                             nextAvailWaitQueueId;

    // All VPs that belong to this process and are currently in a clock_wait()
    struct waitqueue                clk_wait_queue;
    
    // All VPs blocking on a sig_wait() or sig_timedwait()
    struct waitqueue                siwa_queue;
    
    // Signal routes
    queue_t/*struct sigroute>*/     sig_route[SIG_MAX];
    
    // Process termination
    int16_t                         exit_reason;    // Exit code of the first exit() call that initiated the termination of this process
    int16_t                         exit_code;
    vcpu_t _Nullable                exit_coordinator;
} Process;

extern void uwq_destroy(u_wait_queue_t _Nullable self);


extern void Process_Init(ProcessRef _Nonnull self, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, fs_perms_t umask);
extern errno_t Process_CreateChild(ProcessRef _Nonnull self, const proc_spawnattr_t* _Nonnull attr, FileHierarchyRef _Nullable ovrFh, ProcessRef _Nullable * _Nonnull pOutChild);
extern errno_t Process_ApplyActions(ProcessRef _Nonnull self, const proc_spawn_actions_t* _Nonnull actions);


// Returns true if the process is the root process
#define Process_IsRoot(__self) ((__self)->pid == 1)

#define _proc_is_incubating(__self) \
(((__self)->flags & PROC_FLAG_INCUBATING) == PROC_FLAG_INCUBATING)

extern void _proc_terminate(ProcessRef _Nonnull _Locked self, int signo);
extern void _proc_suspend(ProcessRef _Nonnull _Locked self);
extern void _proc_resume(ProcessRef _Nonnull _Locked self);

extern void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self);
extern void _proc_reap_vcpus(ProcessRef _Nonnull self);


extern void _proc_init_default_sigroutes(ProcessRef _Nonnull _Locked self);
extern void _proc_destroy_sigroutes(ProcessRef _Nonnull _Locked self);

#define _proc_set_exit_reason(__self, __reason, __code) \
if ((__self)->exit_reason == 0) {\
    (__self)->exit_reason = __reason; \
    (__self)->exit_code = __code; \
}

#endif /* ProcessPriv_h */
