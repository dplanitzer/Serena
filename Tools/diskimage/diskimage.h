//
//  diskimage.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef diskimage_h
#define diskimage_h

#include "diskimage_types.h"
#include "FauxDisk.h"
#include <assert.h>


extern TimeInterval MonotonicClock_GetCurrentTime(void);
extern errno_t Filesystem_Create(ClassRef _Nonnull pClass, FilesystemRef _Nullable * _Nonnull pOutFileSys);


extern errno_t kalloc(size_t nbytes, void** pOutPtr);
extern void kfree(void* ptr);


#define Bytes_ClearRange(__p, __nbytes) \
    memset(__p, 0, __nbytes)


typedef SRWLOCK Lock; 

extern void Lock_Init(Lock* _Nonnull pLock);
extern void Lock_Deinit(Lock* _Nonnull pLock);
extern void Lock_Lock(Lock* _Nonnull pLock);
extern void Lock_Unlock(Lock* _Nonnull pLock);


typedef CONDITION_VARIABLE ConditionVariable;

extern void ConditionVariable_Init(ConditionVariable* _Nonnull pCondVar);
extern void ConditionVariable_Deinit(ConditionVariable* _Nonnull pCondVar);
extern void ConditionVariable_BroadcastAndUnlock(ConditionVariable* _Nonnull pCondVar, Lock* _Nullable pLock);
extern errno_t ConditionVariable_Wait(ConditionVariable* _Nonnull pCondVar, Lock* _Nonnull pLock, TimeInterval deadline);

#endif /* diskimage_h */
