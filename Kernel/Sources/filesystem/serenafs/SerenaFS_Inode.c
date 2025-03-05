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
#include <System/ByteOrder.h>


errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, sfs_insertion_hint_t* _Nullable pDirInsertionHint, uid_t uid, gid_t gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const TimeInterval curTime = FSGetCurrentTime();
    ino_t parentInodeId = Inode_GetId(dir);
    LogicalBlockAddress inodeLba = 0;
    LogicalBlockAddress dirContLba = 0;
    off_t fileSize = 0ll;
    InodeRef pNode = NULL;
    DiskBlockRef pBlock = NULL;

    if (type == kFileType_Directory) {
        // Make sure that the parent directory is able to accept one more link
        if (Inode_GetLinkCount(dir) >= kSFSLimit_LinkMax) {
            throw(EMLINK);
        }
    }

    try(SfsAllocator_Allocate(&self->blockAllocator, &inodeLba));
    
    if (type == kFileType_Directory) {
        // Write the initial directory content. These are just the '.' and '..'
        // entries
        try(SfsAllocator_Allocate(&self->blockAllocator, &dirContLba));

        try(FSContainer_AcquireBlock(fsContainer, dirContLba, kAcquireBlock_Cleared, &pBlock));
        sfs_dirent_t* dep = DiskBlock_GetMutableData(pBlock);
        dep[0].id = UInt32_HostToBig(inodeLba);
        dep[0].len = 1;
        dep[0].filename[0] = '.';
        dep[1].id = UInt32_HostToBig(parentInodeId);
        dep[1].len = 2;
        dep[1].filename[0] = '.';
        dep[1].filename[1] = '.';
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        pBlock = NULL;

        fileSize = 2 * sizeof(sfs_dirent_t);
    }

    try(SfsAllocator_CommitToDisk(&self->blockAllocator, fsContainer));


    try(FSContainer_AcquireBlock(fsContainer, inodeLba, kAcquireBlock_Cleared, &pBlock));
    sfs_inode_t* ip = (sfs_inode_t*)DiskBlock_GetMutableData(pBlock);
    ip->signature = UInt32_HostToBig(kSFSSignature_Inode);
    ip->id = UInt32_HostToBig(inodeLba);
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = ip->accessTime.tv_sec;
    ip->modificationTime.tv_nsec = ip->accessTime.tv_nsec;
    ip->statusChangeTime.tv_sec = ip->accessTime.tv_sec;
    ip->statusChangeTime.tv_nsec = ip->accessTime.tv_nsec;
    ip->size = Int64_HostToBig(fileSize);
    ip->uid = UInt32_HostToBig(uid);
    ip->gid = UInt32_HostToBig(gid);
    ip->linkCount = Int32_HostToBig(1);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = type;
    ip->bmap.direct[0] = UInt32_HostToBig(dirContLba);
    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    pBlock = NULL;


    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, (ino_t)inodeLba, &pNode));
    try(SfsDirectory_InsertEntry(dir, name, Inode_GetId(pNode), pDirInsertionHint));

    if (type == kFileType_Directory) {
        // Increment the parent directory link count to account for the '..' entry
        // in the just created subdirectory
        Inode_Link(dir);
    }

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

errno_t SerenaFS_onReadNodeFromDisk(SerenaFSRef _Nonnull self, ino_t id, InodeRef _Nullable * _Nonnull pOutNode)
{
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const LogicalBlockAddress lba = (LogicalBlockAddress)id;
    Class* pClass;
    DiskBlockRef pBlock;

    errno_t err = FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_ReadOnly, &pBlock);
    if (err != EOK) {
        *pOutNode = NULL;
        return err;
    }
    
    const sfs_inode_t* ip = (const sfs_inode_t*)DiskBlock_GetData(pBlock);
    if (UInt32_BigToHost(ip->signature) != kSFSSignature_Inode || UInt32_BigToHost(ip->id) != id) {
        FSContainer_RelinquishBlock(fsContainer, pBlock);
        *pOutNode = NULL;
        return EIO;
    }

    switch (ip->type) {
        case kFileType_Directory:
            pClass = class(SfsDirectory);
            break;

        case kFileType_RegularFile:
            pClass = class(SfsRegularFile);
            break;

        default:
            pClass = NULL;
            err = EIO;
            break;
    }

    if (pClass) {
        err = SfsFile_Create(pClass, self, id, ip, pOutNode);
    }
    FSContainer_RelinquishBlock(fsContainer, pBlock);

    return err;
}

errno_t SerenaFS_onWriteNodeToDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);
    DiskBlockRef pBlock;

    const errno_t err = FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Replace, &pBlock);
    if (err == EOK) {
        SfsFile_Serialize(pNode, (sfs_inode_t*)DiskBlock_GetMutableData(pBlock));
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    }

    return err;
}

void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);
    FSContainerRef fsContainer = Filesystem_GetContainer(self);

    SfsFile_Trim((SfsFileRef)pNode, 0ll);
    SfsAllocator_Deallocate(&self->blockAllocator, lba);
    SfsAllocator_CommitToDisk(&self->blockAllocator, fsContainer);
}
