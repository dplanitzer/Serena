//
//  DfsDirectory.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsDirectory_h
#define DfsDirectory_h

#include <filesystem/Inode.h>
#include "DfsItem.h"

    
open_class(DfsDirectory, Inode,
    DfsDirectoryItem* _Nonnull  item;
);
open_class_funcs(DfsDirectory, Inode,
);


extern errno_t DfsDirectory_Create(DevFSRef _Nonnull fs, ino_t inid, DfsDirectoryItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void DfsDirectory_Serialize(InodeRef _Nonnull _Locked pNode);

#define DfsDirectory_GetItem(__self) \
    ((DfsDirectoryRef)__self)->item

#endif /* DfsDirectory_h */
