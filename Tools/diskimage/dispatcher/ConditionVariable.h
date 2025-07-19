//
//  ConditionVariable.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_ConditionVariable_h
#define di_ConditionVariable_h

#include <sched/mtx.h>
#include <kern/errno.h>
#include <time.h>


typedef CONDITION_VARIABLE ConditionVariable;

extern void ConditionVariable_Init(ConditionVariable* pCondVar);
extern void ConditionVariable_Deinit(ConditionVariable* pCondVar);
extern void ConditionVariable_Signal(ConditionVariable* pCondVar);
extern void ConditionVariable_Broadcast(ConditionVariable* pCondVar);
extern errno_t ConditionVariable_Wait(ConditionVariable* pCondVar, mtx_t* mtx);
extern errno_t ConditionVariable_TimedWait(ConditionVariable* pCondVar, mtx_t* mtx, const struct timespec* _Nonnull deadline);

#endif /* di_ConditionVariable_h */
