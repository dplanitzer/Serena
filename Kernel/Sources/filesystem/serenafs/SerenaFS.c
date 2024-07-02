//
//  SerenaFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


// Formats the given disk drive and installs a SerenaFS with an empty root
// directory on it. 'user' and 'permissions' are the user and permissions that
// should be assigned to the root directory.
errno_t SerenaFS_FormatDrive(DiskDriverRef _Nonnull pDriver, User user, FilePermissions permissions)
{
    decl_try_err();
    const size_t diskBlockSize = DiskDriver_GetBlockSize(pDriver);
    const LogicalBlockCount diskBlockCount = DiskDriver_GetBlockCount(pDriver);
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();

    // Make sure that the  disk is compatible with our FS
    if (diskBlockSize != kSFSBlockSize) {
        return EINVAL;
    }
    if (diskBlockCount < kSFSVolume_MinBlockCount) {
        return ENOSPC;
    }


    // Structure of the initialized FS:
    // LBA  
    // 0        Volume Header Block
    // 1        Allocation Bitmap Block #0
    // .        ...
    // Nab      Allocation Bitmap Block #Nab-1
    // Nab+1    Root Directory Inode
    // Nab+2    Root Directory Contents Block #0
    // Nab+3    Unused
    // .        ...
    // Figure out the size and location of the allocation bitmap and root directory
    const uint32_t allocationBitmapByteSize = (diskBlockCount + 7) >> 3;
    const LogicalBlockCount allocBitmapBlockCount = (allocationBitmapByteSize + (diskBlockSize - 1)) / diskBlockSize;
    const LogicalBlockAddress rootDirInodeLba = allocBitmapBlockCount + 1;
    const LogicalBlockAddress rootDirContentLba = rootDirInodeLba + 1;

    uint8_t* p = NULL;
    try(kalloc(diskBlockSize, (void**)&p));


    // Write the volume header
    memset(p, 0, diskBlockSize);
    SFSVolumeHeader* vhp = (SFSVolumeHeader*)p;
    vhp->signature = UInt32_HostToBig(kSFSSignature_SerenaFS);
    vhp->version = UInt32_HostToBig(kSFSVersion_Current);
    vhp->attributes = UInt32_HostToBig(0);
    vhp->creationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->creationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    vhp->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    vhp->blockSize = UInt32_HostToBig(diskBlockSize);
    vhp->volumeBlockCount = UInt32_HostToBig(diskBlockCount);
    vhp->allocationBitmapByteSize = UInt32_HostToBig(allocationBitmapByteSize);
    vhp->rootDirectoryLba = UInt32_HostToBig(rootDirInodeLba);
    vhp->allocationBitmapLba = UInt32_HostToBig(1);
    try(DiskDriver_PutBlock(pDriver, vhp, 0));


    // Write the allocation bitmap
    // Note that we mark the blocks that we already know are in use as in-use
    const size_t nAllocationBitsPerBlock = diskBlockSize << 3;
    const LogicalBlockAddress nBlocksToAllocate = 1 + allocBitmapBlockCount + 1 + 1; // volume header + alloc bitmap + root dir inode + root dir content
    LogicalBlockAddress nBlocksAllocated = 0;

    for (LogicalBlockAddress i = 0; i < allocBitmapBlockCount; i++) {
        memset(p, 0, diskBlockSize);

        LogicalBlockAddress bitNo = 0;
        while (nBlocksAllocated < __min(nBlocksToAllocate, nAllocationBitsPerBlock)) {
            AllocationBitmap_SetBlockInUse(p, bitNo, true);
            nBlocksAllocated++;
            bitNo++;
        }

        try(DiskDriver_PutBlock(pDriver, p, 1 + i));
    }


    // Write the root directory inode
    memset(p, 0, diskBlockSize);
    SFSInode* ip = (SFSInode*)p;
    ip->accessTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(curTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(curTime.tv_nsec);
    ip->size = Int64_HostToBig(2 * sizeof(SFSDirectoryEntry));
    ip->uid = UInt32_HostToBig(user.uid);
    ip->gid = UInt32_HostToBig(user.gid);
    ip->linkCount = Int32_HostToBig(1);
    ip->permissions = UInt16_HostToBig(permissions);
    ip->type = kFileType_Directory;
    ip->bp[0] = UInt32_HostToBig(rootDirContentLba);
    try(DiskDriver_PutBlock(pDriver, ip, rootDirInodeLba));


    // Write the root directory content. This is just the entries '.' and '..'
    // which both point back to the root directory.
    memset(p, 0, diskBlockSize);
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)p;
    dep[0].id = UInt32_HostToBig(rootDirInodeLba);
    dep[0].filename[0] = '.';
    dep[1].id = UInt32_HostToBig(rootDirInodeLba);
    dep[1].filename[0] = '.';
    dep[1].filename[1] = '.';
    try(DiskDriver_PutBlock(pDriver, dep, rootDirContentLba));

catch:
    kfree(p);
    return err;
}

// Creates an instance of SerenaFS. SerenaFS is a volatile file system that does not
// survive system restarts.
errno_t SerenaFS_Create(SerenaFSRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    SerenaFSRef self;

    assert(sizeof(SFSVolumeHeader) <= kSFSBlockSize);
    assert(sizeof(SFSInode) <= kSFSBlockSize);
    assert(sizeof(SFSDirectoryEntry) * kSFSDirectoryEntriesPerBlock == kSFSBlockSize);
    
    try(Filesystem_Create(&kSerenaFSClass, (FilesystemRef*)&self));
    Lock_Init(&self->allocationLock);
    SELock_Init(&self->seLock);
    Lock_Init(&self->moveLock);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void SerenaFS_deinit(SerenaFSRef _Nonnull self)
{
    Lock_Deinit(&self->moveLock);
    SELock_Deinit(&self->seLock);
    Lock_Deinit(&self->allocationLock);
}

errno_t SerenaFS_onMount(SerenaFSRef _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize)
{
    decl_try_err();

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }

    if (self->flags.isMounted) {
        throw(EIO);
    }

    // Make sure that the disk partition actually contains a SerenaFS that we
    // know how to handle.
    if (DiskDriver_GetBlockCount(pDriver) < kSFSVolume_MinBlockCount) {
        throw(EIO);
    }
    if (DiskDriver_GetBlockSize(pDriver) != kSFSBlockSize) {
        throw(EIO);
    }

    try(DiskDriver_GetBlock(pDriver, self->tmpBlock, 0));
    const SFSVolumeHeader* vhp = (const SFSVolumeHeader*)self->tmpBlock;
    const uint32_t signature = UInt32_BigToHost(vhp->signature);
    const uint32_t version = UInt32_BigToHost(vhp->version);
    const uint32_t blockSize = UInt32_BigToHost(vhp->blockSize);
    const uint32_t volumeBlockCount = UInt32_BigToHost(vhp->volumeBlockCount);
    const uint32_t allocationBitmapByteSize = UInt32_BigToHost(vhp->allocationBitmapByteSize);

    if (signature != kSFSSignature_SerenaFS || version != kSFSVersion_v0_1) {
        throw(EIO);
    }
    if (blockSize != kSFSBlockSize || volumeBlockCount < kSFSVolume_MinBlockCount || allocationBitmapByteSize < 1) {
        throw(EIO);
    }

    const size_t diskBlockSize = blockSize;
    size_t allocBitmapByteSize = allocationBitmapByteSize;


    // Cache the root directory info
    self->rootDirLba = UInt32_BigToHost(vhp->rootDirectoryLba);


    // Cache the allocation bitmap in RAM
    self->allocationBitmapLba = UInt32_BigToHost(vhp->allocationBitmapLba);
    self->allocationBitmapBlockCount = (allocBitmapByteSize + (diskBlockSize - 1)) / diskBlockSize;
    self->allocationBitmapByteSize = allocBitmapByteSize;
    self->volumeBlockCount = volumeBlockCount;

    try(kalloc(allocBitmapByteSize, (void**)&self->allocationBitmap));
    uint8_t* pAllocBitmap = self->allocationBitmap;

    for (LogicalBlockAddress lba = 0; lba < self->allocationBitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(kSFSBlockSize, allocBitmapByteSize);

        try(DiskDriver_GetBlock(pDriver, self->tmpBlock, self->allocationBitmapLba + lba));
        memcpy(pAllocBitmap, self->tmpBlock, nBytesToCopy);
        allocBitmapByteSize -= nBytesToCopy;
        pAllocBitmap += diskBlockSize;
    }


    // Store the disk driver reference
    self->diskDriver = Object_RetainAs(pDriver, DiskDriver);
    self->flags.isMounted = 1;
    // XXX should be drive->is_readonly || mount-params->is_readonly
    if (DiskDriver_IsReadOnly(pDriver)) {
        self->flags.isReadOnly = 1;
    }
    else {
        self->flags.isReadOnly = 0;
    }
    
catch:
    SELock_Unlock(&self->seLock);
    return err;
}

errno_t SerenaFS_onUnmount(SerenaFSRef _Nonnull self)
{
    decl_try_err();

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }
    if (!self->flags.isMounted) {
        throw(EIO);
    }
    if (!Filesystem_CanUnmount((FilesystemRef)self)) {
        throw(EBUSY);
    }

    // XXX flush all still cached file data to disk (synchronously)

    // XXX flush the allocation bitmap to disk (synchronously)
    // XXX free the allocation bitmap and clear self->volumeBlockCount

    // XXX clear rootDirLba
    
    Object_Release(self->diskDriver);
    self->diskDriver = NULL;
    self->flags.isMounted = 0;
    self->flags.isReadOnly = 0;

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

bool SerenaFS_isReadOnly(SerenaFSRef _Nonnull self)
{
    return (self->flags.isReadOnly) ? true : false;
}

static errno_t SerenaFS_unlinkCore(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pDir)
{
    decl_try_err();

    // Remove the directory entry in the parent directory
    try(SerenaFS_RemoveDirectoryEntry(self, pDir, Inode_GetId(pNodeToUnlink)));
    SerenaFS_xTruncateFile(self, pDir, Inode_GetFileSize(pDir));


    // If this is a directory then unlink it from its parent since we remove a
    // '..' entry that points to the parent
    if (Inode_IsDirectory(pNodeToUnlink)) {
        Inode_Unlink(pDir);
    }


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);
    Inode_SetModified(pNodeToUnlink, kInodeFlag_StatusChanged);

catch:
    return err;
}

// Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t SerenaFS_unlink(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pDir, User user)
{
    decl_try_err();

    // We must have write permissions for 'pParentNode'
    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Writable));


    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(pNodeToUnlink) && Inode_GetLinkCount(pNodeToUnlink) > 1 && DirectoryNode_IsNotEmpty(pNodeToUnlink)) {
        throw(EBUSY);
    }


    try(SerenaFS_unlinkCore(self, pNodeToUnlink, pDir));

catch:
    return err;
}

// Returns true if the function can establish that 'pDir' is a subdirectory of
// 'pAncestorDir' or that it is in fact 'pAncestorDir' itself.
static bool SerenaFS_IsAncestorOfDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pAncestorDir, InodeRef _Nonnull _Locked pGrandAncestorDir, InodeRef _Nonnull _Locked pDir, User user)
{
    InodeRef pCurDir = Inode_Reacquire(pDir);
    bool r = false;

    while (true) {
        InodeRef pParentDir = NULL;
        bool didLock = false;

        if (Inode_Equals(pCurDir, pAncestorDir)) {
            r = true;
            break;
        }

        if (pCurDir != pDir && pCurDir != pAncestorDir && pCurDir != pGrandAncestorDir) {
            Inode_Lock(pCurDir);
            didLock = true;
        }
        const errno_t err = Filesystem_AcquireNodeForName(self, pCurDir, &kPathComponent_Parent, user, NULL, &pParentDir);
        if (didLock) {
            Inode_Unlock(pCurDir);
        }

        if (err != EOK || Inode_Equals(pCurDir, pParentDir)) {
            // Hit the root directory or encountered an error
            Inode_Relinquish(pParentDir);
            break;
        }

        Inode_Relinquish(pCurDir);
        pCurDir = pParentDir;
    }

catch:
    Inode_Relinquish(pCurDir);
    return r;
}

static errno_t SerenaFS_link(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, User user, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try(SerenaFS_InsertDirectoryEntry(self, pDstDir, pName, Inode_GetId(pSrcNode), (SFSDirectoryEntryPointer*)pDirInstHint->data));
    Inode_Link(pSrcNode);
    Inode_SetModified(pSrcNode, kInodeFlag_StatusChanged);

    return EOK;

catch:
    return err;
}

errno_t SerenaFS_move(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, User user, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();
    const bool isMovingDir = Inode_IsDirectory(pSrcNode);

    // The 'moveLock' ensures that there can be only one operation active at any
    // given time that might move directories around in the filesystem. This ie
    // ensures that the result that we get from calling IsAscendentOfDirectory()
    // stays meaningful while we are busy executing the move.
    Lock_Lock(&self->moveLock);

    if (isMovingDir && SerenaFS_IsAncestorOfDirectory(self, pSrcNode, pSrcDir, pDstDir, user)) {
        // oldpath is an ancestor of newpath (Don't allow moving a directory inside of itself)
        throw(EINVAL);
    }


    // Add a new entry in the destination directory and remove the old entry from
    // the source directory
    try(SerenaFS_link(self, pSrcNode, pDstDir, pNewName, user, pDirInstHint));
    try(SerenaFS_unlinkCore(self, pSrcNode, pSrcDir));  // XXX should theoretically be able to use unlink() here. Fails with resource busy because we trigger the empty check on the destination directory


    // If we're moving a directory then we need to repoint its parent entry '..'
    // to the new parent directory
    if (isMovingDir) {
        SFSDirectoryEntryPointer mp;
        SFSDirectoryQuery q;

        q.kind = kSFSDirectoryQuery_PathComponent;
        q.u.pc = &kPathComponent_Parent;
        try(SerenaFS_GetDirectoryEntry(self, pSrcNode, &q, NULL, &mp, NULL, NULL));

        try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, mp.lba));

        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + mp.blockOffset);
        dep->id = Inode_GetId(pDstDir);

        try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, mp.lba));

        // Our parent receives a +1 on the link count because of our .. entry
        Inode_Link(pDstDir);
    }

catch:
    Lock_Unlock(&self->moveLock);
    return err;
}

errno_t SerenaFS_rename(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, User user)
{
    decl_try_err();
    SFSDirectoryEntryPointer mp;
    SFSDirectoryQuery q;

    if (pNewName->count > kSFSMaxFilenameLength) {
        throw(ENAMETOOLONG);
    }
    
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = Inode_GetId(pSrcNode);
    try(SerenaFS_GetDirectoryEntry(self, pSrcDir, &q, NULL, &mp, NULL, NULL));

    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, mp.lba));

    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + mp.blockOffset);
    char* p = String_CopyUpTo(dep->filename, pNewName->name, pNewName->count);
    while (p < &dep->filename[kSFSMaxFilenameLength]) *p++ = '\0';

    try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, mp.lba));

    return EOK;

catch:
    return err;
}


class_func_defs(SerenaFS, Filesystem,
override_func_def(deinit, SerenaFS, Object)
override_func_def(onReadNodeFromDisk, SerenaFS, Filesystem)
override_func_def(onWriteNodeToDisk, SerenaFS, Filesystem)
override_func_def(onRemoveNodeFromDisk, SerenaFS, Filesystem)
override_func_def(onMount, SerenaFS, Filesystem)
override_func_def(onUnmount, SerenaFS, Filesystem)
override_func_def(isReadOnly, SerenaFS, Filesystem)
override_func_def(acquireRootDirectory, SerenaFS, Filesystem)
override_func_def(acquireNodeForName, SerenaFS, Filesystem)
override_func_def(getNameOfNode, SerenaFS, Filesystem)
override_func_def(createNode, SerenaFS, Filesystem)
override_func_def(openFile, SerenaFS, Filesystem)
override_func_def(readFile, SerenaFS, Filesystem)
override_func_def(writeFile, SerenaFS, Filesystem)
override_func_def(truncateFile, SerenaFS, Filesystem)
override_func_def(openDirectory, SerenaFS, Filesystem)
override_func_def(readDirectory, SerenaFS, Filesystem)
override_func_def(unlink, SerenaFS, Filesystem)
override_func_def(move, SerenaFS, Filesystem)
override_func_def(rename, SerenaFS, Filesystem)
);
