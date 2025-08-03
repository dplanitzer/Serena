//
//  KfsDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef KfsDevice_h
#define KfsDevice_h

#include "KfsNode.h"
#include <driver/Driver.h>

    
open_class(KfsDevice, KfsNode,
    DriverRef _Nonnull  instance;
    intptr_t            arg;
);
open_class_funcs(KfsDevice, KfsNode,
);


extern errno_t KfsDevice_Create(KernFSRef _Nonnull fs, ino_t inid, mode_t mode, uid_t uid, gid_t gid, ino_t pnid, DriverRef _Nonnull pDriver, intptr_t arg, KfsNodeRef _Nullable * _Nonnull pOutSelf);

#endif /* KfsDevice_h */
