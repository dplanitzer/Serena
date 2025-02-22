//
//  SfsFile.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsFile_h
#define SfsFile_h

#include <disk/DiskBlock.h>
#include <filesystem/Inode.h>
#include "VolumeFormat.h"


open_class(SfsFile, Inode,
    sfs_bno_t   direct[kSFSInodeBlockPointersCount];
);
open_class_funcs(SfsFile, Inode,
);


extern errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, InodeId inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip);

extern errno_t SfsFile_AcquireBlock(SfsFileRef _Nonnull _Locked self, sfs_bno_t fba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock);
extern void SfsFile_ConvertOffset(SfsFileRef _Nonnull _Locked self, FileOffset offset, sfs_bno_t* _Nonnull pOutBlockIdx, ssize_t* _Nonnull pOutBlockOffset);

extern void SfsFile_xTruncate(SfsFileRef _Nonnull _Locked self, FileOffset newLength);

// Returns the top-level of the inode's associated block map. Note that all block
// addresses in the block map are in big-endian byte order (even in core memory).
#define SfsFile_GetBlockMap(__self) \
((SfsFileRef)__self)->direct

#endif /* SfsFile_h */
