//
//  SerenaFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


// Creates an instance of SerenaFS.
errno_t SerenaFS_Create(FSContainerRef _Nonnull pContainer, SerenaFSRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    SerenaFSRef self;

    assert(sizeof(SFSVolumeHeader) <= kSFSBlockSize);
    assert(sizeof(SFSInode) <= kSFSBlockSize);
    assert(sizeof(SFSDirectoryEntry) * kSFSDirectoryEntriesPerBlock == kSFSBlockSize);
    
    try(ContainerFilesystem_Create(&kSerenaFSClass, pContainer, (FilesystemRef*)&self));
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

errno_t SerenaFS_start(SerenaFSRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    FSContainerInfo fscInfo;
    DiskBlockRef pRootBlock = NULL;

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }

    if (self->mountFlags.isMounted) {
        throw(EIO);
    }

    // Make sure that the disk partition actually contains a SerenaFS that we
    // know how to handle.
    try(FSContainer_GetInfo(fsContainer, &fscInfo));

    if (fscInfo.blockCount < kSFSVolume_MinBlockCount) {
        throw(EIO);
    }
    if (fscInfo.blockSize != kSFSBlockSize) {
        throw(EIO);
    }


    // Establish the default mount settings
    self->mountFlags.isAccessUpdateOnReadEnabled = 1;
    self->mountFlags.isReadOnly = 0;


    // Get the FS root block
    try(FSContainer_AcquireBlock(fsContainer, 0, kAcquireBlock_ReadOnly, &pRootBlock));
    const SFSVolumeHeader* vhp = (const SFSVolumeHeader*)DiskBlock_GetData(pRootBlock);
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

    try(FSAllocate(allocBitmapByteSize, (void**)&self->allocationBitmap));
    uint8_t* pAllocBitmap = self->allocationBitmap;

    for (LogicalBlockAddress lba = 0; lba < self->allocationBitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(kSFSBlockSize, allocBitmapByteSize);
        DiskBlockRef pBlock;

        try(FSContainer_AcquireBlock(fsContainer, self->allocationBitmapLba + lba, kAcquireBlock_ReadOnly, &pBlock));
        memcpy(pAllocBitmap, DiskBlock_GetData(pBlock), nBytesToCopy);
        FSContainer_RelinquishBlock(fsContainer, pBlock);
        pBlock = NULL;

        allocBitmapByteSize -= nBytesToCopy;
        pAllocBitmap += diskBlockSize;
    }


    self->mountFlags.isMounted = 1;
    // XXX should be drive->is_readonly || mount-params->is_readonly
    if (fscInfo.isReadOnly) {
        self->mountFlags.isReadOnly = 1;
    }
    
#ifdef __SERENA__
    // XXX disabled updates to the access dates for now as long as there's no
    // XXX disk cache and no boot-from-HD support
    self->mountFlags.isAccessUpdateOnReadEnabled = 0;
#endif

catch:
    FSContainer_RelinquishBlock(fsContainer, pRootBlock);

    SELock_Unlock(&self->seLock);
    return err;
}

errno_t SerenaFS_stop(SerenaFSRef _Nonnull self)
{
    decl_try_err();

    if ((err = SELock_LockExclusive(&self->seLock)) != EOK) {
        return err;
    }
    if (!self->mountFlags.isMounted) {
        throw(EIO);
    }
    if (!Filesystem_CanUnmount((FilesystemRef)self)) {
        throw(EBUSY);
    }

    // XXX flush all still cached file data to disk (synchronously)

    // XXX flush the allocation bitmap to disk (synchronously)
    // XXX free the allocation bitmap and clear self->volumeBlockCount

    // XXX clear rootDirLba
    
    self->mountFlags.isMounted = 0;

catch:
    SELock_Unlock(&self->seLock);
    return err;
}

bool SerenaFS_isReadOnly(SerenaFSRef _Nonnull self)
{
    return (self->mountFlags.isReadOnly) ? true : false;
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

    // We must have write permissions for 'pDir'
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

errno_t SerenaFS_link(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pName, User user, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try(SerenaFS_InsertDirectoryEntry(self, pDstDir, pName, Inode_GetId(pSrcNode), (SFSDirectoryEntryPointer*)pDirInstHint->data));
    Inode_Link(pSrcNode);
    Inode_SetModified(pSrcNode, kInodeFlag_StatusChanged);

catch:
    return err;
}

errno_t SerenaFS_move(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, User user, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
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
        DiskBlockRef pBlock;

        q.kind = kSFSDirectoryQuery_PathComponent;
        q.u.pc = &kPathComponent_Parent;
        try(SerenaFS_GetDirectoryEntry(self, pSrcNode, &q, NULL, &mp, NULL, NULL));

        try(FSContainer_AcquireBlock(fsContainer, mp.lba, kAcquireBlock_Update, &pBlock));

        uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(bp + mp.blockOffset);
        dep->id = Inode_GetId(pDstDir);

        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

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
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    SFSDirectoryEntryPointer mp;
    SFSDirectoryQuery q;
    DiskBlockRef pBlock;

    if (pNewName->count > kSFSMaxFilenameLength) {
        throw(ENAMETOOLONG);
    }
    
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = Inode_GetId(pSrcNode);
    try(SerenaFS_GetDirectoryEntry(self, pSrcDir, &q, NULL, &mp, NULL, NULL));

    try(FSContainer_AcquireBlock(fsContainer, mp.lba, kAcquireBlock_Update, &pBlock));

    uint8_t* bp = DiskBlock_GetMutableData(pBlock);
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(bp + mp.blockOffset);
    char* p = String_CopyUpTo(dep->filename, pNewName->name, pNewName->count);
    while (p < &dep->filename[kSFSMaxFilenameLength]) *p++ = '\0';

    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

    return EOK;

catch:
    return err;
}


class_func_defs(SerenaFS, ContainerFilesystem,
override_func_def(deinit, SerenaFS, Object)
override_func_def(onReadNodeFromDisk, SerenaFS, Filesystem)
override_func_def(onWriteNodeToDisk, SerenaFS, Filesystem)
override_func_def(onRemoveNodeFromDisk, SerenaFS, Filesystem)
override_func_def(start, SerenaFS, Filesystem)
override_func_def(stop, SerenaFS, Filesystem)
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
