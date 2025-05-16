//
//  SerenaFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include "SfsDirectory.h"
#include "SfsRegularFile.h"
#include <kern/endian.h>


errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, sfs_insertion_hint_t* _Nullable pDirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const struct timespec curTime = FSGetCurrentTime();
    ino_t parentInodeId = Inode_GetId(dir);
    blkno_t inodeLba = 0;
    blkno_t dirContLba = 0;
    off_t fileSize = 0ll;
    InodeRef pNode = NULL;
    FSBlock blk = {0};

    try(SfsDirectory_CanAcceptEntry(dir, name, type));

    try(SfsAllocator_Allocate(&self->blockAllocator, &inodeLba));
    
    if (type == S_IFDIR) {
        // Write the initial directory content. These are just the '.' and '..'
        // entries
        try(SfsAllocator_Allocate(&self->blockAllocator, &dirContLba));

        try(FSContainer_MapBlock(fsContainer, dirContLba, kMapBlock_Cleared, &blk));
        sfs_dirent_t* dep = (sfs_dirent_t*)blk.data;
        dep[0].id = UInt32_HostToBig(inodeLba);
        dep[0].len = 1;
        dep[0].filename[0] = '.';
        dep[1].id = UInt32_HostToBig(parentInodeId);
        dep[1].len = 2;
        dep[1].filename[0] = '.';
        dep[1].filename[1] = '.';
        FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);
        blk.token = 0;

        fileSize = 2 * sizeof(sfs_dirent_t);
    }


    try(FSContainer_MapBlock(fsContainer, inodeLba, kMapBlock_Cleared, &blk));
    sfs_inode_t* ip = (sfs_inode_t*)blk.data;
    ip->size = Int64_HostToBig(fileSize);
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = ip->accessTime.tv_sec;
    ip->modificationTime.tv_nsec = ip->accessTime.tv_nsec;
    ip->statusChangeTime.tv_sec = ip->accessTime.tv_sec;
    ip->statusChangeTime.tv_nsec = ip->accessTime.tv_nsec;
    ip->signature = UInt32_HostToBig(kSFSSignature_Inode);
    ip->id = UInt32_HostToBig(inodeLba);
    ip->pnid = UInt32_HostToBig(parentInodeId);
    ip->linkCount = Int32_HostToBig(1);
    ip->uid = UInt32_HostToBig(uid);
    ip->gid = UInt32_HostToBig(gid);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = type;
    ip->bmap.direct[0] = UInt32_HostToBig(dirContLba);
    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);
    blk.token = 0;


    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, (ino_t)inodeLba, &pNode));
    Inode_Lock(pNode);
    err = SfsDirectory_InsertEntry(dir, name, pNode, pDirInsertionHint);
    if (err == EOK) {
        Inode_Writeback(dir);
    }
    Inode_Unlock(pNode);
    throw_iferr(err);

    try(SfsAllocator_CommitToDisk(&self->blockAllocator, fsContainer));

    *pOutNode = pNode;
    return EOK;

catch:
    if (pNode) {
        Filesystem_Unlink((FilesystemRef)self, pNode, dir);
        Filesystem_RelinquishNode((FilesystemRef)self, pNode);
    }

    if (dirContLba != 0) {
        SfsAllocator_Deallocate(&self->blockAllocator, dirContLba);
    }
    if (inodeLba != 0) {
        SfsAllocator_Deallocate(&self->blockAllocator, inodeLba);
    }
    SfsAllocator_CommitToDisk(&self->blockAllocator, fsContainer);
    *pOutNode = NULL;

    return err;
}

errno_t SerenaFS_onAcquireNode(SerenaFSRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const blkno_t lba = (blkno_t)id;
    Class* pClass;
    InodeRef pNode = NULL;
    FSBlock blk = {0};

    try(FSContainer_MapBlock(fsContainer, lba, kMapBlock_ReadOnly, &blk));
    
    const sfs_inode_t* ip = (const sfs_inode_t*)blk.data;
    if (UInt32_BigToHost(ip->signature) != kSFSSignature_Inode || UInt32_BigToHost(ip->id) != id) {
        throw(EIO);
    }

    switch (ip->type) {
        case S_IFDIR:
            pClass = class(SfsDirectory);
            break;

        case S_IFREG:
            pClass = class(SfsRegularFile);
            break;

        default:
            throw(EIO);
            break;
    }

    err = SfsFile_Create(pClass, self, id, ip, &pNode);

catch:
    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_None);
    *pOutNode = pNode;
    return err;
}

errno_t SerenaFS_onWritebackNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const blkno_t lba = (blkno_t)Inode_GetId(pNode);
    const bool doDelete = (Inode_GetLinkCount(pNode) == 0) ? true : false;
    FSBlock blk = {0};

    // Remove the file content if the file should be deleted
    if (doDelete) {
        // linkCount == 0 at this point
        SfsFile_Trim((SfsFileRef)pNode, 0ll);
        Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }


    // Write the inode meta-data back to disk
    const errno_t err = FSContainer_MapBlock(fsContainer, lba, kMapBlock_Replace, &blk);
    if (err == EOK) {
        SfsFile_Serialize(pNode, (sfs_inode_t*)blk.data);
        FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);
    }


    // Free the inode block and flush the modified allocation bitmap back to
    // disk if we delete the inode
    if (doDelete) {
        SfsAllocator_Deallocate(&self->blockAllocator, lba);
        SfsAllocator_CommitToDisk(&self->blockAllocator, fsContainer);
    }

    return err;
}
