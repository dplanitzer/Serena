//
//  DfsDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsDevice_h
#define DfsDevice_h

#include "DfsNode.h"
#include <driver/Driver.h>

    
open_class(DfsDevice, DfsNode,
    DriverRef _Nonnull  instance;
    intptr_t            arg;
);
open_class_funcs(DfsDevice, DfsNode,
);


extern errno_t DfsDevice_Create(DevFSRef _Nonnull fs, ino_t inid, FilePermissions permissions, uid_t uid, gid_t gid, ino_t pnid, DriverRef _Nonnull pDriver, intptr_t arg, DfsNodeRef _Nullable * _Nonnull pOutSelf);

#endif /* DfsDevice_h */
