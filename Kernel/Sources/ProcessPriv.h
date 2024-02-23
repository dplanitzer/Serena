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
#include "PathResolver.h"


// A process tombstone is created by a process that voluntarily or involuntarily
// exits. It records the PID and status of the exiting process. The tombstone is
// added to the parent process of the process that exits.
// This data structure is created by the exiting (child) process and is then
// handed over to the parent process which takes ownership. Once this happens
// the data structure is protected by the parent's lock.
typedef struct _ProcessTombstone {
    ListNode    node;
    int         pid;        // PID of process that exited
    int         status;     // Exit status
} ProcessTombstone;


// Must be >= 3
#define INITIAL_IOCHANNELS_SIZE 64

CLASS_IVARS(Process, Object,
    Lock                        lock;
    
    ProcessId                   ppid;       // parent's PID
    ProcessId                   pid;        // my PID

    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;

    // Resources
    ObjectArray                 ioChannels;

    // Filesystems/Namespace
    PathResolver                pathResolver;
    FilePermissions             fileCreationMask;   // Mask of file permissions that should be filtered out from user supplied permissions when creating a file system object
    
    // User identity
    User                        realUser;       // User identity inherited from the parent process / set at spawn time
    
    // Process image
    char* _Nullable _Weak       imageBase;      // Base address to the contiguous memory region holding exec header, text, data and bss segments
    char* _Nullable _Weak       argumentsBase;  // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // Process termination
    AtomicBool                  isTerminating;  // true if the process is going through the termination process
    int                         exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child process related properties
    IntArray                    childPids;      // PIDs of all my child processes
    List                        tombstones;     // Tombstones of child processes that have terminated and have not yet been consumed by waitpid()
    ConditionVariable           tombstoneSignaler;
);


extern errno_t Process_Create(ProcessId ppid, User user, InodeRef _Nonnull pRootDir, InodeRef _Nonnull pCurDir, FilePermissions fileCreationMask, ProcessRef _Nullable * _Nonnull pOutProc);
extern void Process_deinit(ProcessRef _Nonnull pProc);

extern errno_t Process_RegisterIOChannel_Locked(ProcessRef _Nonnull pProc, IOChannelRef _Nonnull pChannel, int* _Nonnull pOutDescriptor);

// Closes all registered I/O channels. Ignores any errors that may be returned
// from the close() call of a channel.
extern void Process_CloseAllIOChannels_Locked(ProcessRef _Nonnull pProc);

// Frees all tombstones
extern void Process_DestroyAllTombstones_Locked(ProcessRef _Nonnull pProc);

// Returns true if the process is the root process
#define Process_IsRoot(__pProc) (pProc->pid == 1)

// Creates a new tombstone for the given child process with the given exit status
extern errno_t Process_OnChildDidTerminate(ProcessRef _Nonnull pProc, ProcessId childPid, int childExitCode);

// Runs on the kernel main dispatch queue and terminates the given process.
extern void _Process_DoTerminate(ProcessRef _Nonnull pProc);

// Adopts the process wth the given PID as a child. The ppid of 'pOtherProc' must
// be the PID of the receiver.
extern errno_t Process_AdoptChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid);

// Abandons the process with the given PID as a child of the receiver.
extern void Process_AbandonChild_Locked(ProcessRef _Nonnull pProc, ProcessId childPid);

// Loads an executable from the given executable file into the process address
// space.
// \param pProc the process into which the executable image should be loaded
// \param pExecAddr pointer to a GemDOS formatted executable file in memory
// \param pArgv the command line arguments for the process. NULL means that the arguments are {path, NULL}
// \param pEnv the environment for the process. Null means that the process inherits the environment from its parent
// XXX expects that the address space is empty at call time
// XXX the executable format is GemDOS
// XXX the executable file must be located at the address 'pExecAddr'
extern errno_t Process_Exec_Locked(ProcessRef _Nonnull pProc, void* _Nonnull pExecAddr, const char* const _Nullable * _Nullable pArgv, const char* const _Nullable * _Nullable pEnv);

#endif /* ProcessPriv_h */
