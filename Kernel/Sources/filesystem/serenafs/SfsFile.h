//
//  SfsFile.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsFile_h
#define SfsFile_h

#include <filesystem/Inode.h>
#include "VolumeFormat.h"


open_class(SfsFile, Inode,
    SFSBlockNumber  direct[kSFSInodeBlockPointersCount];
);
open_class_funcs(SfsFile, Inode,
);


extern errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, InodeId inid, const SFSInode* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, SFSInode* _Nonnull ip);

// Returns the top-level of the inode's associated block map. Note that all block
// addresses in the block map are in big-endian byte order (even in core memory).
#define SfsFile_GetBlockMap(__self) \
((SfsFileRef)__self)->direct

#endif /* SfsFile_h */
