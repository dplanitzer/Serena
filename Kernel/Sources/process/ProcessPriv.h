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
#include <Catalog.h>
#include <filemanager/FileManager.h>
#include <machine/rc.h>
#include <sched/mtx.h>
#include <sched/waitqueue.h>
#include <vm/AddressSpace.h>


// Process states
#define PROC_LIFECYCLE_ACTIVE       0
#define PROC_LIFECYCLE_ZOMBIFYING   1
#define PROC_LIFECYCLE_ZOMBIE       2


// Userspace wait queues
#define UWQ_HASH_CHAIN_COUNT    4
#define UWQ_HASH_CHAIN_MASK     (UWQ_HASH_CHAIN_COUNT - 1)

typedef struct UWaitQueue {
    ListNode            qe;
    struct waitqueue    wq;
    unsigned int        policy;
    int                 id;
} UWaitQueue;


typedef struct proc_img {
    AddressSpace        as;
    vcpu_t _Nullable    main_vp;
    void* _Nullable     base;
    void* _Nullable     entry_point;
    char* _Nullable     pargs;
} proc_img_t;


// Process relationship information (owned & protected by ProcessManager)
typedef struct proc_rel {
    SListNode                       pid_qe;     // pid_table chain entry.
    SList/*<Process>*/              children;
    SListNode                       child_qe;
} proc_rel_t;



typedef struct Process {
    proc_rel_t                      rel;        // process relationships maintained by the process manager. Must be the first field here

    ref_count_t                     retainCount;
    mtx_t                           mtx;

    pid_t                           pid;        // my PID
    pid_t                           ppid;       // parent's PID
    pid_t                           pgrp;       // Group id. I'm the group leader if pgrp == pid
    pid_t                           sid;        // (Login) session id. I'm the session leader if sid == pid 
    CatalogId                       catalogId;  // proc-fs catalog id

    // Process lifecycle state
    int                             state;

    // Process image
    AddressSpace                    addr_space;
    char* _Nullable _Weak           pargs_base; // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // VPs
    List                            vcpu_queue;     // List of VPs [mtx]
    size_t                          vcpu_count;
    vcpuid_t                        next_avail_vcpuid;

    // I/O Channels
    IOChannelTable                  ioChannelTable;     // I/O channels (aka sharable resources)
    
    // File manager
    FileManager                     fm;

    // UWaitQueues
    List/*<UWaitQueue>*/            waitQueueTable[UWQ_HASH_CHAIN_COUNT];   // wait queue descriptor -> UWaitQueue
    int                             nextAvailWaitQueueId;

    // All VPs that belong to this process and are currently in sleep()
    struct waitqueue                sleep_queue;
    
    // All VPs blocking on a sigwait() or sigtimedwait()
    struct waitqueue                siwa_queue;
    
    // Exceptions support
    excpt_handler_t                 excpt_handler;
    
    // Process termination
    int16_t                         exit_reason;    // Exit code of the first exit() call that initiated the termination of this process
    int16_t                         exit_code;
} Process;

extern void uwq_destroy(UWaitQueue* _Nullable self);


// Creates a new process with a unique pid. 'ppid' is the id of teh parent process
// and must be provided. 'pgrp' is an exiting process group id if > 0; if == 0
// then the new process will be the leader of a new process group with a group
// id equal to its pid. Same for 'sid'.
extern errno_t Process_Create(pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull fh, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf);
extern void Process_deinit(ProcessRef _Nonnull self);

// Returns true if the process is the root process
#define Process_IsRoot(__self) ((__self)->pid == 1)

extern errno_t Process_Publish(ProcessRef _Locked _Nonnull self);
extern errno_t Process_Unpublish(ProcessRef _Locked _Nonnull self);

extern void _proc_abort_other_vcpus(ProcessRef _Nonnull _Locked self);
extern void _proc_reap_vcpus(ProcessRef _Nonnull self);

#endif /* ProcessPriv_h */
