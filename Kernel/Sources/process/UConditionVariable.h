//
//  UConditionVariable.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef UConditionVariable_h
#define UConditionVariable_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <dispatcher/ConditionVariable.h>
#include "ULock.h"


OPEN_CLASS_WITH_REF(UConditionVariable, Object,
    ConditionVariable   cv;
);
typedef struct UConditionVariableMethodTable {
    ObjectMethodTable   super;
} UConditionVariableMethodTable;


// Creates a condition variable suitable for use by userspace code.
extern errno_t UConditionVariable_Create(UConditionVariableRef _Nullable * _Nonnull pOutSelf);

// Wakes the given condition variable and unlock the associated lock if
// '__pLock' is not NULL. This wakes up one or all waiters.
#define UConditionVariable_WakeAndUnlock(__self, __pLock, __broadcast) \
ConditionVariable_WakeAndUnlock(&(__self)->cv, &(__pLock)->lock, __broadcast)

// Blocks the caller until the condition variable has received a signal or the
// wait has timed out. Automatically and atomically acquires the associated
// lock on wakeup. An ETIMEOUT error is returned if teh condition variable is
// not signaled before 'deadline'.
#define UConditionVariable_Wait(__self, __pLock, __deadline) \
ConditionVariable_Wait(&(__self)->cv, &(__pLock)->lock, __deadline)

#endif /* UConditionVariable_h */
