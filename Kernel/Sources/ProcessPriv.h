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


typedef struct _Process {
    Int                         pid;
    Lock                        lock;

    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;

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


#endif /* ProcessPriv_h */
