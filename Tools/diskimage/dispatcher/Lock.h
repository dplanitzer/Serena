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

extern void Lock_Init(Lock* self);
extern void Lock_Deinit(Lock* self);
extern void Lock_Lock(Lock* self);
extern void Lock_Unlock(Lock* self);

#endif /* di_Lock_h */
