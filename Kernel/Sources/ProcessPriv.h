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


typedef struct _Process {
    Int                         pid;
    DispatchQueueRef _Nonnull   mainDispatchQueue;
    AddressSpaceRef _Nonnull    addressSpace;
    AtomicBool                  isTerminating;  // true if the process is going through the termination process
} Process;


#endif /* ProcessPriv_h */
