//
//  SerenaFS_Inode.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, InodeRef _Nonnull _Locked dir, const PathComponent* _Nonnull name, SFSDirectoryEntryPointer* _Nullable pDirInsertionHint, UserId uid, GroupId gid, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const TimeInterval curTime = FSGetCurrentTime();
    InodeId parentInodeId = Inode_GetId(dir);
    LogicalBlockAddress inodeLba = 0;
    LogicalBlockAddress dirContLba = 0;
    FileOffset fileSize = 0ll;
    InodeRef pNode = NULL;
    DiskBlockRef pBlock = NULL;

    if (type == kFileType_Directory) {
        // Make sure that the parent directory is able to accept one more link
        if (Inode_GetLinkCount(dir) >= kSFSLimit_LinkMax) {
            throw(EMLINK);
        }
    }

    try(BlockAllocator_Allocate(&self->blockAllocator, &inodeLba));
    
    if (type == kFileType_Directory) {
        // Write the initial directory content. These are just the '.' and '..'
        // entries
        try(BlockAllocator_Allocate(&self->blockAllocator, &dirContLba));

        try(FSContainer_AcquireBlock(fsContainer, dirContLba, kAcquireBlock_Cleared, &pBlock));
        SFSDirectoryEntry* dep = DiskBlock_GetMutableData(pBlock);
        dep[0].id = UInt32_HostToBig(inodeLba);
        dep[0].filename[0] = '.';
        dep[1].id = UInt32_HostToBig(parentInodeId);
        dep[1].filename[0] = '.';
        dep[1].filename[1] = '.';
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        pBlock = NULL;

        fileSize = 2 * sizeof(SFSDirectoryEntry);
    }

    try(BlockAllocator_CommitToDisk(&self->blockAllocator, fsContainer));


    try(FSContainer_AcquireBlock(fsContainer, inodeLba, kAcquireBlock_Cleared, &pBlock));
    SFSInode* ip = (SFSInode*)DiskBlock_GetMutableData(pBlock);
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
    ip->bp[0] = UInt32_HostToBig(dirContLba);
    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    pBlock = NULL;


    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, (InodeId)inodeLba, &pNode));
    try(SerenaFS_InsertDirectoryEntry(self, dir, name, Inode_GetId(pNode), pDirInsertionHint));

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
        BlockAllocator_Deallocate(&self->blockAllocator, dirContLba);
    }
    if (inodeLba != 0) {
        BlockAllocator_Deallocate(&self->blockAllocator, inodeLba);
    }
    BlockAllocator_CommitToDisk(&self->blockAllocator, fsContainer);
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
        class(Inode),
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

static void SerenaFS_DeallocateFileContentBlocks(SerenaFSRef _Nonnull self, FSContainerRef _Nonnull fsContainer, InodeRef _Nonnull pNode)
{
    decl_try_err();
    DiskBlockRef pBlock;
    const SFSBlockNumber* l0_bmap = (const SFSBlockNumber*)Inode_GetBlockMap(pNode);

    if (l0_bmap[kSFSDirectBlockPointersCount] != 0) {
        if ((err = FSContainer_AcquireBlock(fsContainer, UInt32_BigToHost(l0_bmap[kSFSDirectBlockPointersCount]), kAcquireBlock_Update, &pBlock)) == EOK) {
            SFSBlockNumber* l1_bmap = (SFSBlockNumber*)DiskBlock_GetData(pBlock);

            for (int i = 0; i < kSFSBlockPointersPerBlockCount; i++) {
                if (l1_bmap[i] != 0) {
                    BlockAllocator_Deallocate(&self->blockAllocator, UInt32_BigToHost(l1_bmap[i]));
                }
            }

            FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        }
        BlockAllocator_Deallocate(&self->blockAllocator, UInt32_BigToHost(l0_bmap[kSFSDirectBlockPointersCount]));
    }

    for (int i = 0; i < kSFSDirectBlockPointersCount; i++) {
        if (l0_bmap[i] != 0) {
            BlockAllocator_Deallocate(&self->blockAllocator, UInt32_BigToHost(l0_bmap[i]));
        }
    }
}

void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);
    FSContainerRef fsContainer = Filesystem_GetContainer(self);

    SerenaFS_DeallocateFileContentBlocks(self, fsContainer, pNode);
    BlockAllocator_Deallocate(&self->blockAllocator, lba);
    BlockAllocator_CommitToDisk(&self->blockAllocator, fsContainer);
}
