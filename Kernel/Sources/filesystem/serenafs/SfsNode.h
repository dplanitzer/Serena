//
//  SfsNode.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsNode_h
#define SfsNode_h

#include <filesystem/Inode.h>
#include "VolumeFormat.h"


open_class(SfsNode, Inode,
    SFSBlockNumber  direct[kSFSInodeBlockPointersCount];
);
open_class_funcs(SfsNode, Inode,
);


extern errno_t SfsNode_Create(SerenaFSRef _Nonnull fs, InodeId inid, const SFSInode* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void SfsNode_Serialize(InodeRef _Nonnull _Locked pNode, SFSInode* _Nonnull ip);

// Returns the top-level of the inode's associated block map. Note that all block
// addresses in the block map are in big-endian byte order (even in core memory).
#define SfsNode_GetBlockMap(__self) \
((SfsNodeRef)__self)->direct

#endif /* SfsNode_h */
