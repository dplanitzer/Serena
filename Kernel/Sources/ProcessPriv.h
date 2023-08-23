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
#include "Atomic.h"
#include "DispatchQueue.h"
#include "List.h"
#include "Lock.h"


typedef struct _Process {
    Int                         pid;
    Lock                        lock;

    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;

    // Process termination
    AtomicBool                  isTerminating;  // true if the process is going through the termination process
    Int                         exitCode;       // Exit code of the first exit() call that initiated the termination of this process

    // Child processes (protected by 'lock')
    List                        children;
    ListNode                    siblings;
    ProcessRef _Nullable _Weak  parent;
} Process;


#endif /* ProcessPriv_h */
