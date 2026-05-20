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


// Signal routes
struct sigroute {
    deque_node_t    qe;
    id_t            target_id;
    int16_t         use_count;
    int8_t          signo;
    int8_t          target;
};
typedef struct sigroute* sigroute_t;


// The reason for a state change
typedef struct state_change_reason {
    int      reason;    // reason for state change; 0 if no reason has been recorded yet or proc_waitstate() has already consumed it
    union {
        int exit_code;  // child process exit code
        int signo;      // signal that caused the process to terminate
        int excptno;    // exception that caused the process to terminate
    }           u;
} state_change_reason_t;

#define _WAIT_REASON_NONE  0


// Process relationship information (owned & protected by ProcessManager)
typedef struct proc_rel {
    queue_node_t            pid_qe;     // pid_table chain entry.
    queue_t/*<Process>*/    children;
    queue_node_t            child_qe;
} proc_rel_t;


// Process flags
#define PROC_FLAG_TERMINATING   0x01    /* Process_Terminate() has been called */
#define PROC_FLAG_USER          0x02    /* This process is owned by the kernel (e.g. special privileges apply; vcpus do not acquire a user stack) */


typedef struct Process {
    proc_rel_t                      rel;        // process relationships maintained by the process manager. Must be the first field here

    ref_count_t                     retainCount;
    mtx_t                           mtx;

    pid_t                           pid;        // my PID
    pid_t                           ppid;       // parent's PID
    pid_t                           pgrp;       // Group id. I'm the group leader if pgrp == pid
    pid_t                           sid;        // (Login) session id. I'm the session leader if sid == pid 

    // Process state management
    vcpu_t _Nullable                terminator_vcpu;    // vcpu coordinating process termination
    vcpu_t _Nullable                stopper_vcpu;       // vcpu coordinator a stop operation
    int8_t                          run_state;
    state_change_reason_t           run_state_reason;   // reason why we entered the current run state
    int8_t                          signo_causing_termination;  // original signal that will lead to a SIG_FORCE_QUIT
    uint8_t                         flags;

    // Process image
    AddressSpace                    addr_space;
    proc_ctx_t* _Nullable           ctx_base;       // Base address to the contiguous memory region holding the proc_ctx_t structure, command line arguments and environment. NULL until the first exec()
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

    // All VPs that belong to this process and are currently in a clock_sleep()
    struct waitqueue                clk_wait_queue;
    
    // All VPs blocking on a sig_wait()
    struct waitqueue                siwa_queue;
    
    // Signal routes
    deque_t/*<struct sigroute>*/    sig_routes;
} Process;


extern void Process_Init(ProcessRef _Nonnull self, ProcessRef _Locked _Nullable parent, bool isUser, FileHierarchyRef _Nonnull fh, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir);

// Creates a new child user process and publishes it to the process manager.
// This function returns a strong reference to the new process. The caller should
// release this strong reference when no longer needed. Note that the process
// manager maintains a strong reference to all living processes. This reference
// keeps them alive.
extern errno_t Process_CreateUserChild(ProcessRef _Nonnull self, const proc_spawnattr_t* _Nonnull attr, FileHierarchyRef _Nullable ovrFh, ProcessRef _Nullable * _Nonnull pOutChild);

// Applies the given list of spawn actions to the process.
extern errno_t Process_ApplyActions(ProcessRef _Nonnull self, const proc_spawn_actions_t* _Nonnull actions, ProcessRef _Nonnull parent, size_t* _Nonnull pOutFailedActionIndex);


extern bool _proc_is_terminating(ProcessRef _Nonnull self);

#define _proc_is_user(__self) \
(((__self)->flags & PROC_FLAG_USER) == PROC_FLAG_USER)

extern errno_t _proc_acquire_vcpu(ProcessRef _Nonnull _Locked self, const _vcpu_acquire_attr_t* _Nonnull attr, bool isMain, vcpu_t _Nullable * _Nonnull pOutVp);

extern void _proc_set_state(ProcessRef _Nonnull _Locked self, int state, int reason, intptr_t arg, bool notify_parent);

extern void _proc_stop(ProcessRef _Nonnull _Locked self, int reason, int arg, bool notify_parent);
extern void _proc_continue(ProcessRef _Nonnull _Locked self, int reason, int arg, bool notify_parent);

extern void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self);
extern void _proc_reap_vcpus(ProcessRef _Nonnull self);

extern void _proc_destroy_sigroutes(ProcessRef _Nonnull _Locked self);
extern void _proc_destroy_sigroutes_except_for_vcpuid(ProcessRef _Nonnull _Locked self, vcpuid_t vid);
extern void _proc_reassign_sigroutes_to_vcpuid(ProcessRef _Nonnull _Locked self, vcpuid_t oldid, vcpuid_t newid);

extern vcpu_t _Nullable _proc_vcpu_for_id(ProcessRef _Nonnull self, vcpuid_t id);

#endif /* ProcessPriv_h */
