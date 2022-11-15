//
//  ConditionVariable.h
//  Apollo
//
//  Created by Dietmar Planitzer on 4/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef ConditionVariable_h
#define ConditionVariable_h

#include "Foundation.h"
#include "List.h"
#include "Lock.h"
#include "MonotonicClock.h"


// A condition variable
typedef struct _ConditionVariable {
    List        wait_queue;
    Character   name[8];
} ConditionVariable;


extern ConditionVariable* _Nullable ConditionVariable_Create(void);
extern void ConditionVariable_Destroy(ConditionVariable* _Nullable pCondVar);

extern void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar);
extern void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar);

extern void ConditionVariable_Signal(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock);
extern void ConditionVariable_Broadcast(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock);
extern ErrorCode ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline);

#endif /* ConditionVariable_h */
