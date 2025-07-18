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
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <filemanager/FileManager.h>
#include <sched/waitqueue.h>
#include <vm/AddressSpace.h>


// Process states
#define PS_ALIVE      0
#define PS_ZOMBIFYING 1
#define PS_ZOMBIE     2


// Userspace wait queues
#define UWQ_HASH_CHAIN_COUNT    4
#define UWQ_HASH_CHAIN_MASK     (UWQ_HASH_CHAIN_COUNT - 1)

typedef struct UWaitQueue {
    ListNode            qe;
    struct waitqueue    wq;
    unsigned int        policy;
    int                 id;
} UWaitQueue;


final_class_ivars(Process, Object,
    Lock                            lock;
    
    ListNode                        ptce;       // Process table chain entry. Protected by ProcessManager lock

    int                             state;
    pid_t                           pid;        // my PID
    pid_t                           ppid;       // parent's PID
    pid_t                           pgrp;       // Group id. I'm the group leader if pgrp == pid
    pid_t                           sid;        // (Login) session id. I'm the session leader if sid == pid 
    CatalogId                       catalogId;  // proc-fs catalog id

    // VPs
    List                            vpQueue;    // List of VPs [lock]

    AddressSpaceRef _Nonnull        addressSpace;

    // I/O Channels
    IOChannelTable                  ioChannelTable;     // I/O channels (aka sharable resources)
    
    // File manager
    FileManager                     fm;

    // UWaitQueues
    List/*<UWaitQueue>*/            waitQueueTable[UWQ_HASH_CHAIN_COUNT];   // wait queue descriptor -> UWaitQueue
    int                             nextAvailWaitQueueId;

    // All VPs that belong to this process and are currently in sleep()
    struct waitqueue                sleepQueue;
    
    // All VPs blocking on a sigwait() or sigtimedwait()
    struct waitqueue                siwaQueue;
    
    // Process image
    char* _Nullable _Weak           imageBase;      // Base address to the contiguous memory region holding exec header, text, data and bss segments
    char* _Nullable _Weak           argumentsBase;  // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // Process termination
    int                             exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child process related properties
    List/*<Process>*/               children;
    ListNode                        siblings;
    ConditionVariable               procTermSignaler;
);

#define proc_from_ptce(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(struct Process, ptce))

#define proc_from_siblings(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(struct Process, siblings))

extern void uwq_destroy(UWaitQueue* _Nullable self);


extern errno_t Process_Create(pid_t pid, pid_t ppid, pid_t pgrp, pid_t sid, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf);
extern void Process_deinit(ProcessRef _Nonnull self);

// Returns true if the process is the root process
#define Process_IsRoot(__self) ((__self)->pid == 1)

// Adopts the given process as a child. The ppid of 'child' must be the PID of
// the receiver.
extern void Process_AdoptChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child);

// Abandons the given process as a child of the receiver.
extern void Process_AbandonChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child);

extern errno_t Process_Publish(ProcessRef _Locked _Nonnull self);
extern errno_t Process_Unpublish(ProcessRef _Locked _Nonnull self);

#endif /* ProcessPriv_h */
