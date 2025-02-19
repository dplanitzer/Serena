//
//  DfsDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsDevice_h
#define DfsDevice_h

#include <filesystem/Inode.h>
#include "DfsItem.h"

    
open_class(DfsDevice, Inode,
    DfsDeviceItem* _Nonnull item;
);
open_class_funcs(DfsDevice, Inode,
);


extern errno_t DfsDevice_Create(DevFSRef _Nonnull fs, InodeId inid, DfsDeviceItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void DfsDevice_Serialize(InodeRef _Nonnull _Locked pNode);

#define DfsDevice_GetItem(__self) \
    ((DfsDeviceRef)__self)->item

#endif /* DfsDevice_h */
