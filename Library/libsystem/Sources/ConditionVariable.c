//
//  ConditionVariable.c
//  libsystem
//
//  Created by Dietmar Planitzer on 3/21/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include <System/ConditionVariable.h>
#include <System/_syscall.h>
#include "LockPriv.h"

#define CV_SIGNATURE 0x53454d41

// Must be sizeof(UConditionVariable) <= 16 
typedef struct UConditionVariable {
    int             od;
    unsigned int    signature;
    int             r2;
    int             r3;
} UConditionVariable;


errno_t ConditionVariable_Init(ConditionVariableRef _Nonnull cv)
{
    UConditionVariable* self = (UConditionVariable*)cv;

    self->signature = 0;
    self->r2 = 0;
    self->r3 = 0;

    const errno_t err = _syscall(SC_cv_create, &self->od);
    if (err == EOK) {
        self->signature = CV_SIGNATURE;
    }

    return err;
}

errno_t ConditionVariable_Deinit(ConditionVariableRef _Nonnull cv)
{
    UConditionVariable* self = (UConditionVariable*)cv;

    if (self->signature != CV_SIGNATURE) {
        return EINVAL;
    }

    const errno_t err = _syscall(SC_dispose, self->od);
    self->signature = 0;
    self->od = 0;

    return err;
}

errno_t ConditionVariable_Signal(ConditionVariableRef _Nonnull cv, LockRef _Nullable lock)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    ULock* ulock = (ULock*)lock;

    if (self->signature == CV_SIGNATURE && ulock->signature == LOCK_SIGNATURE) {
        return _syscall(SC_cv_wake, self->od, ulock->od, 0);
    }
    else {
        return EINVAL;
    }
}

errno_t ConditionVariable_Broadcast(ConditionVariableRef _Nonnull cv, LockRef _Nullable lock)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    ULock* ulock = (ULock*)lock;

    if (self->signature == CV_SIGNATURE && ulock->signature == LOCK_SIGNATURE) {
        return _syscall(SC_cv_wake, self->od, ulock->od, 1);
    }
    else {
        return EINVAL;
    }
}

errno_t ConditionVariable_Wait(ConditionVariableRef _Nonnull cv, LockRef _Nonnull lock, TimeInterval deadline)
{
    UConditionVariable* self = (UConditionVariable*)cv;
    ULock* ulock = (ULock*)lock;

    if (self->signature == CV_SIGNATURE && ulock->signature == LOCK_SIGNATURE) {
        return _syscall(sc_cv_wait, self->od, ulock->od, deadline);
    }
    else {
        return EINVAL;
    }
}
