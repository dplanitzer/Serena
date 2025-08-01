//
//  DriverManager.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverManager_h
#define DriverManager_h

#include <kern/errno.h>
#include <kern/types.h>
#include <kobj/AnyRefs.h>

typedef struct DriverManager {
    DriverRef   platformController;
    DriverRef   hidDriver;
    DriverRef   logDriver;
    DriverRef   nullDriver;
    DriverRef   virtualDiskDriver;
} DriverManager;


extern DriverManagerRef gDriverManager;


extern errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf);

extern errno_t DriverManager_Start(DriverManagerRef _Nonnull self);

#endif /* DriverManager_h */
