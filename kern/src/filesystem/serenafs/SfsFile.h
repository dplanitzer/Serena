//
//  SfsFile.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef SfsFile_h
#define SfsFile_h

#include <ext/try.h>
#include <filesystem/FSBlock.h>
#include <filesystem/Inode.h>
#include <kpi/sefs_format.h>


extern sfs_itype_t SfsITypeFromMode(mode_t mode);
extern mode_t SfsModeFromIType(sfs_itype_t itype);

#define SfsPermissionsFromMode(__mode) \
((__mode) & 0777)

#define SfsModeFromPermissions(__perm) \
(__perm)


typedef struct SfsFileBlock {
    FSBlock     b;
    blkno_t     lba;
    bool        wasAlloced;
    bool        isZeroFill;
} SfsFileBlock;


open_class(SfsFile, Inode,
    sfs_bmap_t  bmap;
);
open_class_funcs(SfsFile, Inode,
);


extern errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, ino_t inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode);
extern void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip);

extern void SfsFile_ConvertOffset(SfsFileRef _Nonnull _Locked self, off_t offset, sfs_bno_t* _Nonnull pOutFba, ssize_t* _Nonnull pOutFbaOffset);

extern errno_t SfsFile_MapBlock(SfsFileRef _Nonnull _Locked self, sfs_bno_t fba, MapBlock mode, SfsFileBlock* _Nonnull blk);
extern errno_t SfsFile_UnmapBlock(SfsFileRef _Nonnull _Locked self, SfsFileBlock* _Nonnull blk, WriteBlock mode);

extern bool SfsFile_Trim(SfsFileRef _Nonnull _Locked self, off_t newLength);

#define SfsFile_GetIType(__self) \
(SfsITypeFromMode(Inode_GetMode(__self)))

#define SfsFile_GetPermissions(__self) \
(SfsPermissionsFromMode(Inode_GetMode(__self)))

#define SfsFile_IsDirectory(__self) \
(S_ISDIR(Inode_GetMode(__self)))

#endif /* SfsFile_h */
