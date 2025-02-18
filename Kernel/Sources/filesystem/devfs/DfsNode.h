//
//  DfsNode.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DfsNode_h
#define DfsNode_h

#include <filesystem/Inode.h>
#include "DfsItem.h"

    
open_class(DfsNode, Inode,
    union {
        DfsItem* _Nonnull           item;
        DfsDriverItem* _Nonnull     driver;
        DfsDirectoryItem* _Nonnull  dir;
    }   u;
);
open_class_funcs(DfsNode, Inode,
);


extern errno_t DfsNode_Create(DevFSRef _Nonnull fs, InodeId inid, DfsItem* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void DfsNode_Serialize(InodeRef _Nonnull _Locked pNode, DfsItem* _Nonnull ip);

extern DriverRef _Nullable DfsNode_CopyDriver(InodeRef _Nonnull pNode);

// Returns the DfsItem backing the inode.
#define DfsNode_GetItem(__self) \
    ((DfsNodeRef)__self)->u.item

#define DfsNode_GetDirectory(__self) \
    ((DfsNodeRef)__self)->u.dir

#define DfsNode_GetDriver(__self) \
    ((DfsNodeRef)__self)->u.driver

#endif /* DfsNode_h */
