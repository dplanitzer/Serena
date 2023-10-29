//
//  ProcessPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 7/12/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ProcessPriv_h
#define ProcessPriv_h

#include "Process.h"
#include "AddressSpace.h"
#include "ConditionVariable.h"
#include "DispatchQueue.h"
#include "Lock.h"


// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
typedef struct __process_arguments_t ProcessArguments;


// A process tombstone is created by a process that voluntarily or involuntarily
// exits. It records the PID and status of the exiting process. The tombstone is
// added to the parent process of the process that exits.
// This data structure is created by the exiting (child) process and is then
// handed over to the parent process which takes ownership. Once this happens
// the data structure is protected by the parent's lock.
typedef struct __ProcessTombstone {
    ListNode    node;
    Int         pid;        // PID of process that exited
    Int         status;     // Exit status
} ProcessTombstone;


// Must be >= 3
#define INITIAL_DESC_TABLE_SIZE 64
#define DESC_TABLE_INCREMENT    128

// XXX revist this: dynamic array, paged array, maybe set
#define CHILD_PROC_CAPACITY 4

CLASS_IVARS(Process, Object,
    Lock                        lock;
    
    Int                         ppid;       // parent's PID
    Int                         pid;        // my PID

    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;

    // IOChannels
    IOChannelRef* _Nonnull      ioChannels;
    Int                         ioChannelsCapacity;
    Int                         ioChannelsCount;

    // Process image
    Byte* _Nullable _Weak       imageBase;      // Base address to the contiguous memory region holding exec header, text, data and bss segments
    Byte* _Nullable _Weak       argumentsBase;  // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // Process termination
    AtomicBool                  isTerminating;  // true if the process is going through the termination process
    Int                         exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child process related properties
    Int* _Nonnull               childPids;      // PIDs of all my child processes
    List                        tombstones;     // Tombstones of child processes that have terminated and have not yet been consumed by waitpid()
    ConditionVariable           tombstoneSignaler;
);


extern ErrorCode Process_Create(Int ppid, ProcessRef _Nullable * _Nonnull pOutProc);
extern void Process_deinit(ProcessRef _Nonnull pProc);

// Unregisters all registered I/O channels. Ignores any errors that may be
// returned from the close() call of a channel.
extern void Process_UnregisterAllIOChannels_Locked(ProcessRef _Nonnull pProc);

// Frees all tombstones
extern void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull pProc);

// Returns true if the process is the root process
#define Process_IsRoot(__pProc) (pProc->pid == 1)

// Creates a new tombstone for the given child process with the given exit status
extern ErrorCode Process_OnChildDidTerminate(ProcessRef _Nonnull pProc, Int childPid, Int childExitCode);

// Runs on the kernel main dispatch queue and terminates the given process.
extern void _Process_DoTerminate(ProcessRef _Nonnull pProc);

// Adopts the process wth the given PID as a child. The ppid of 'pOtherProc' must
// be the PID of the receiver.
extern void Process_AdoptChild_Locked(ProcessRef _Nonnull pProc, Int childPid);

// Abandons the process with the given PID as a child of the receiver.
extern void Process_AbandonChild_Locked(ProcessRef _Nonnull pProc, Int childPid);

// Loads an executable from the given executable file into the process address
// space.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// \param pArgv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param pEnv the environment for teh process. Null means that the process inherits the environment from its parent
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be loacted at the address 'pExecAddr'
extern ErrorCode Process_Exec_Locked(ProcessRef _Nonnull pProc, Byte* _Nonnull pExecAddr, const Character* const _Nullable * _Nullable pArgv, const Character* const _Nullable * _Nullable pEnv);

#endif /* ProcessPriv_h */
