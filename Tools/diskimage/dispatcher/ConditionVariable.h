//
//  ConditionVariable.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_ConditionVariable_h
#define di_ConditionVariable_h

#include "Lock.h"
#include <klib/Error.h>
#include <klib/TimeInterval.h>


typedef CONDITION_VARIABLE ConditionVariable;

extern void ConditionVariable_Init(ConditionVariable* pCondVar);
extern void ConditionVariable_Deinit(ConditionVariable* pCondVar);
extern void ConditionVariable_Broadcast(ConditionVariable* pCondVar);
extern void ConditionVariable_BroadcastAndUnlock(ConditionVariable* pCondVar, Lock* pLock);
extern errno_t ConditionVariable_Wait(ConditionVariable* pCondVar, Lock* pLock, TimeInterval deadline);

#endif /* di_ConditionVariable_h */
