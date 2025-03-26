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

typedef struct sfs_mapblk {
    DiskBlockRef _Nullable  block;
    uint8_t* _Nullable      data;
    LogicalBlockAddress     lba;
    bool                    wasAlloced;
} sfs_mapblk_t;


open_class(SfsFile, Inode,
    sfs_bmap_t  bmap;
);
open_class_funcs(SfsFile, Inode,
);


extern errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, ino_t inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip);

extern void SfsFile_ConvertOffset(SfsFileRef _Nonnull _Locked self, off_t offset, sfs_bno_t* _Nonnull pOutFba, ssize_t* _Nonnull pOutFbaOffset);

extern errno_t SfsFile_AcquireBlock(SfsFileRef _Nonnull _Locked self, sfs_bno_t fba, AcquireBlock mode, sfs_mapblk_t* _Nonnull blk);
extern bool SfsFile_Trim(SfsFileRef _Nonnull _Locked self, off_t newLength);

#endif /* SfsFile_h */
