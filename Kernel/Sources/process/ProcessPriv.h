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
#include "AddressSpace.h"
#include "IOChannelTable.h"
#include "UResourceTable.h"
#include <Catalog.h>
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <dispatcher/WaitQueue.h>
#include <dispatchqueue/DispatchQueue.h>
#include <filemanager/FileManager.h>


// A process tombstone is created by a process that voluntarily or involuntarily
// exits. It records the PID and status of the exiting process. The tombstone is
// added to the parent process of the process that exits.
// This data structure is created by the exiting (child) process and is then
// handed over to the parent process which takes ownership. Once this happens
// the data structure is protected by the parent's lock.
typedef struct ProcessTombstone {
    ListNode    node;
    int         pid;        // PID of process that exited
    int         status;     // Exit status
} ProcessTombstone;


final_class_ivars(Process, Object,
    Lock                            lock;
    
    ListNode                        ptce;       // Process table chain entry. Protected by ProcessManager lock

    pid_t                           ppid;       // parent's PID
    pid_t                           pid;        // my PID
    CatalogId                       catalogId;  // proc-fs catalog id

    DispatchQueueRef _Nonnull _Weak mainDispatchQueue;
    AddressSpaceRef _Nonnull        addressSpace;

    // Resources
    IOChannelTable                  ioChannelTable;     // I/O channels (aka sharable resources)
    UResourceTable                  uResourcesTable;    // Process private resources (aka non-sharable resources)
    
    // File manager
    FileManager                     fm;
    
    // All VPs that belong to this process and are currently in sleep()
    WaitQueue                       sleepQueue;

    // Wait queue used for sig_xxx() functions
    WaitQueue                       siwaQueue;
    
    // Process image
    char* _Nullable _Weak           imageBase;      // Base address to the contiguous memory region holding exec header, text, data and bss segments
    char* _Nullable _Weak           argumentsBase;  // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // Process termination
    AtomicBool                      isTerminating;  // true if the process is going through the termination process
    int                             exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child process related properties
    List/*<Process>*/               children;
    ListNode                        siblings;
    List/*<ProcessTombstone>*/      tombstones;     // Tombstones of child processes that have terminated and have not yet been consumed by waitpid()
    ConditionVariable               tombstoneSignaler;
    DispatchQueueRef _Nullable      terminationNotificationQueue;   // Post the terminationNotificationClosure to this queue on process death, if not NULL
    dispatch_func_t _Nullable       terminationNotificationClosure;
    void* _Nullable                 terminationNotificationContext;
);

#define proc_from_ptce(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(struct Process, ptce))

#define proc_from_siblings(__ptr) \
(ProcessRef) (((uint8_t*)__ptr) - offsetof(struct Process, siblings))



extern errno_t Process_Create(pid_t ppid, FileHierarchyRef _Nonnull pFileHierarchy, uid_t uid, gid_t gid, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pWorkingDir, mode_t umask, ProcessRef _Nullable * _Nonnull pOutSelf);
extern void Process_deinit(ProcessRef _Nonnull self);

// Frees all tombstones
extern void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull self);

// Returns true if the process is the root process
#define Process_IsRoot(__self) ((__self)->pid == 1)

// Called by the given child process tp notify its parent about its death.
// Creates a new tombstone for the given child process with the given exit status
// and posts a termination notification closure if one was provided for the child
// process. Expects that the child process state does not change while this function
// is executing.
extern errno_t Process_OnChildTermination(ProcessRef _Nonnull self, ProcessRef _Nonnull pChildProc);

// Runs on the kernel main dispatch queue and terminates the given process.
extern void _Process_DoTerminate(ProcessRef _Nonnull self);

// Adopts the given process as a child. The ppid of 'child' must be the PID of
// the receiver.
extern void Process_AdoptChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child);

// Abandons the given process as a child of the receiver.
extern void Process_AbandonChild_Locked(ProcessRef _Nonnull self, ProcessRef _Nonnull child);

extern void Process_CallUser(VoidFunc_2 _Nonnull f, void* _Nullable arg);

extern errno_t Process_Publish(ProcessRef _Locked _Nonnull self);
extern errno_t Process_Unpublish(ProcessRef _Locked _Nonnull self);

#endif /* ProcessPriv_h */
