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
#include "DispatchQueue.h"
#include "Lock.h"


// The process arguments descriptor is stored in the process address space and
// it contains a pointer to the base of the command line arguments and environment
// variables tables. These tables store pointers to nul-terminated strings and
// the last entry in the table contains a NULL.
typedef struct __process_arguments_t ProcessArguments;


// Must be >= 3
#define INITIAL_DESC_TABLE_SIZE 64
#define DESC_TABLE_INCREMENT    128


typedef struct _Process {
    Int                         pid;
    Lock                        lock;

    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;

    // UObjects
    IOChannelRef* _Nonnull      ioChannels;
    Int                         ioChannelsCapacity;
    Int                         ioChannelsCount;

    // Process image
    Byte* _Nullable _Weak       imageBase;      // Base address to the contiguous memory region holding exec header, text, data and bss segments
    Byte* _Nullable _Weak       argumentsBase;  // Base address to the contiguous memory region holding the pargs structure, command line arguments and environment

    // Process termination
    AtomicBool                  isTerminating;  // true if the process is going through the termination process
    Int                         exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child processes (protected by 'lock')
    List                        children;
    ListNode                    siblings;
    ProcessRef _Nullable _Weak  parent;
} Process;


extern ErrorCode Process_Create(Int pid, ProcessRef _Nullable * _Nonnull pOutProc);

// Unregisters all registered I/O channels. Ignores any errors that may be
// returned from the close() call of a channel.
extern void Process_UnregisterAllIOChannels_Locked(ProcessRef _Nonnull pProc);

// Adds the given process as a child to the given process. 'pOtherProc' must not
// already be a child of another process.
extern void Process_AddChildProcess_Locked(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);

// Removes the given process from 'pProc'. Does nothing if the given process is
// not a child of 'pProc'.
extern void Process_RemoveChildProcess_Locked(ProcessRef _Nonnull pProc, ProcessRef _Nonnull pOtherProc);

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
