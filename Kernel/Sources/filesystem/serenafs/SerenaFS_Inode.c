//
//  SerenaFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, SFSDirectoryEntryPointer* _Nullable pDirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const TimeInterval curTime = FSGetCurrentTime();
    InodeId parentInodeId = Inode_GetId(pDir);
    LogicalBlockAddress inodeLba = 0;
    LogicalBlockAddress dirContLba = 0;
    FileOffset fileSize = 0ll;
    SFSBlockNumber* bmap = NULL;
    InodeRef pNode = NULL;
    bool isPublished = false;

    // We must have write permissions for the parent directory
    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Writable));


    if (type == kFileType_Directory) {
        // Make sure that the parent directory is able to accept one more link
        if (Inode_GetLinkCount(pDir) >= kSFSLimit_LinkMax) {
            throw(EMLINK);
        }
    }

    try(kalloc_cleared(sizeof(SFSBlockNumber) * kSFSInodeBlockPointersCount, (void**)&bmap));
    try(SerenaFS_AllocateBlock(self, &inodeLba));
    
    if (type == kFileType_Directory) {
        // Write the initial directory content. These are just the '.' and '..'
        // entries
        DiskBlockRef pBlock;

        try(SerenaFS_AllocateBlock(self, &dirContLba));
        bmap[0] = UInt32_HostToBig(dirContLba);

        try(FSContainer_AcquireBlock(fsContainer, dirContLba, kAcquireBlock_Cleared, &pBlock));
        SFSDirectoryEntry* dep = DiskBlock_GetMutableData(pBlock);
        dep[0].id = UInt32_HostToBig(inodeLba);
        dep[0].filename[0] = '.';
        dep[1].id = UInt32_HostToBig(parentInodeId);
        dep[1].filename[0] = '.';
        dep[1].filename[1] = '.';
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

        fileSize = 2 * sizeof(SFSDirectoryEntry);
    }

    try(Inode_Create(
        (FilesystemRef)self,
        (InodeId)inodeLba,
        type,
        1,
        user.uid,
        user.gid,
        permissions,
        fileSize,
        curTime,
        curTime,
        curTime,
        bmap,
        &pNode));
    Inode_SetModified(pNode, kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged);


    // Be sure to publish the newly create inode before we add it to the parent
    // directory. This ensures that a guy who stumbles across the new directory
    // entry and calls Filesystem_AcquireNode() on it, won't unexpectedly create
    // a second inode object representing the same inode.  
    try(Filesystem_PublishNode((FilesystemRef)self, pNode));
    isPublished = true;
    try(SerenaFS_InsertDirectoryEntry(self, pDir, pName, Inode_GetId(pNode), pDirInsertionHint));

    if (type == kFileType_Directory) {
        // Increment the parent directory link count to account for the '..' entry
        // in the just created subdirectory
        Inode_Link(pDir);
    }

    *pOutNode = pNode;

    return EOK;

catch:
    if (isPublished) {
        Filesystem_Unlink((FilesystemRef)self, pNode, pDir, user);
        Filesystem_RelinquishNode((FilesystemRef)self, pNode);
    } else {
        Inode_Destroy(pNode);
    }
    FSDeallocate(bmap);

    if (dirContLba != 0) {
        SerenaFS_DeallocateBlock(self, dirContLba);
    }
    if (inodeLba != 0) {
        SerenaFS_DeallocateBlock(self, inodeLba);
    }
    *pOutNode = NULL;

    return err;
}

errno_t SerenaFS_onReadNodeFromDisk(SerenaFSRef _Nonnull self, InodeId id, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    DiskBlockRef pBlock;
    const LogicalBlockAddress lba = (LogicalBlockAddress)id;
    SFSBlockNumber* bmap = NULL;

    try(FSAllocate(sizeof(SFSBlockNumber) * kSFSInodeBlockPointersCount, (void**)&bmap));
    try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_ReadOnly, &pBlock));
    const SFSInode* ip = (const SFSInode*)DiskBlock_GetData(pBlock);

    memcpy(bmap, ip->bp, sizeof(uint32_t) * kSFSInodeBlockPointersCount);

    err = Inode_Create(
        (FilesystemRef)self,
        id,
        ip->type,
        Int32_BigToHost(ip->linkCount),
        UInt32_BigToHost(ip->uid),
        UInt32_BigToHost(ip->gid),
        UInt16_BigToHost(ip->permissions),
        Int64_BigToHost(ip->size),
        TimeInterval_Make(UInt32_BigToHost(ip->accessTime.tv_sec), UInt32_BigToHost(ip->accessTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->modificationTime.tv_sec), UInt32_BigToHost(ip->modificationTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->statusChangeTime.tv_sec), UInt32_BigToHost(ip->statusChangeTime.tv_nsec)),
        bmap,
        pOutNode);

    FSContainer_RelinquishBlock(fsContainer, pBlock);
    return err;

catch:
    FSDeallocate(bmap);
    *pOutNode = NULL;
    return err;
}

errno_t SerenaFS_onWriteNodeToDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    DiskBlockRef pBlock;
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);
    const SFSBlockNumber* bmap = (const SFSBlockNumber*)Inode_GetBlockMap(pNode);
    const TimeInterval curTime = FSGetCurrentTime();

    try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Cleared, &pBlock));
    SFSInode* ip = (SFSInode*)DiskBlock_GetMutableData(pBlock);
    const TimeInterval accTime = (Inode_IsAccessed(pNode)) ? curTime : Inode_GetAccessTime(pNode);
    const TimeInterval modTime = (Inode_IsUpdated(pNode)) ? curTime : Inode_GetModificationTime(pNode);
    const TimeInterval chgTime = (Inode_IsStatusChanged(pNode)) ? curTime : Inode_GetStatusChangeTime(pNode);

    ip->accessTime.tv_sec = UInt32_HostToBig(accTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(accTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(modTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(modTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(chgTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(chgTime.tv_nsec);
    ip->size = Int64_HostToBig(Inode_GetFileSize(pNode));
    ip->uid = UInt32_HostToBig(Inode_GetUserId(pNode));
    ip->gid = UInt32_HostToBig(Inode_GetGroupId(pNode));
    ip->linkCount = Int32_HostToBig(Inode_GetLinkCount(pNode));
    ip->permissions = UInt16_HostToBig(Inode_GetFilePermissions(pNode));
    ip->type = Inode_GetFileType(pNode);

    memcpy(ip->bp, bmap, sizeof(uint32_t) * kSFSInodeBlockPointersCount);

    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

catch:
    return err;
}

static void SerenaFS_DeallocateFileContentBlocks(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    DiskBlockRef pBlock;
    const SFSBlockNumber* l0_bmap = (const SFSBlockNumber*)Inode_GetBlockMap(pNode);

    if (l0_bmap[kSFSDirectBlockPointersCount] != 0) {
        if ((err = FSContainer_AcquireBlock(fsContainer, UInt32_BigToHost(l0_bmap[kSFSDirectBlockPointersCount]), kAcquireBlock_ReadOnly, &pBlock)) == EOK) {
            SFSBlockNumber* l1_bmap = (SFSBlockNumber*)DiskBlock_GetData(pBlock);

            for (int i = 0; i < kSFSBlockPointersPerBlockCount; i++) {
                if (l1_bmap[i] != 0) {
                    SerenaFS_DeallocateBlock(self, UInt32_BigToHost(l1_bmap[i]));
                }
            }

            FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        }
        SerenaFS_DeallocateBlock(self, UInt32_BigToHost(l0_bmap[kSFSDirectBlockPointersCount]));
    }

    for (int i = 0; i < kSFSDirectBlockPointersCount; i++) {
        if (l0_bmap[i] != 0) {
            SerenaFS_DeallocateBlock(self, UInt32_BigToHost(l0_bmap[i]));
        }
    }
}

void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);

    SerenaFS_DeallocateFileContentBlocks(self, pNode);
    SerenaFS_DeallocateBlock(self, lba);
}
