//
//  Lock.h
//  diskimage
//
//  Created by Dietmar Planitzer on 3/10/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef di_Lock_h
#define di_Lock_h

#include <windows.h>

typedef SRWLOCK Lock; 

extern void Lock_Init(Lock* pLock);
extern void Lock_Deinit(Lock* pLock);
extern void Lock_Lock(Lock* pLock);
extern void Lock_Unlock(Lock* pLock);

#endif /* di_Lock_h */
