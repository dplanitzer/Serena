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

    try(ContainerFilesystem_Create(&kSerenaFSClass, pContainer, (FilesystemRef*)&self));
    Lock_Init(&self->moveLock);
    SfsAllocator_Init(&self->blockAllocator);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void SerenaFS_deinit(SerenaFSRef _Nonnull self)
{
    FSDeallocate(self->emptyReadOnlyBlock);
    self->emptyReadOnlyBlock = NULL;
    
    Lock_Deinit(&self->moveLock);
    SfsAllocator_Deinit(&self->blockAllocator);
}

errno_t SerenaFS_onStart(SerenaFSRef _Nonnull self, const void* _Nonnull pParams, ssize_t paramsSize, FSProperties* _Nonnull pOutProps)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    FSBlock blk = {0};

    // Make sure that the disk partition actually contains a SerenaFS that we
    // know how to handle.
    const LogicalBlockCount fscBlockCount = FSContainer_GetBlockCount(fsContainer);
    const size_t fscBlockSize = FSContainer_GetBlockSize(fsContainer);
    const bool fscIsReadOnly = FSContainer_IsReadOnly(fsContainer);

    if (fscBlockCount < kSFSVolume_MinBlockCount) {
        throw(EIO);
    }
    if (fscBlockSize > UINT16_MAX) {
        throw(EIO);
    }


    // Establish the default settings
    self->mountFlags.isAccessUpdateOnReadEnabled = 1;


    // Get the FS root block
    try(FSContainer_MapBlock(fsContainer, 0, kMapBlock_ReadOnly, &blk));
    const sfs_vol_header_t* vhp = (const sfs_vol_header_t*)blk.data;
    const uint32_t signature = UInt32_BigToHost(vhp->signature);
    const uint32_t version = UInt32_BigToHost(vhp->version);
    const uint32_t blockSize = UInt32_BigToHost(vhp->volBlockSize);

    if (signature != kSFSSignature_SerenaFS || version != kSFSVersion_v0_1) {
        throw(EIO);
    }
    if (blockSize != fscBlockSize) {
        throw(EIO);
    }


    // Allocate an empty read-only block for zero-fill reads
    try(FSAllocateCleared(blockSize, (void**)&self->emptyReadOnlyBlock));

    
    // Get the root directory id
    pOutProps->rootDirectoryId = (ino_t) UInt32_BigToHost(vhp->lbaRootDir);
    pOutProps->isReadOnly = false;


    // Cache the allocation bitmap in RAM
    try(SfsAllocator_Start(&self->blockAllocator, fsContainer, vhp, blockSize));


    // Calculate various parameters that depend on the concrete disk block size
    self->blockSize = blockSize;
    self->blockShift = FSLog2(blockSize);
    self->blockMask = blockSize - 1;
    self->indirectBlockEntryCount = blockSize / sizeof(sfs_bno_t);


    // XXX should be drive->is_readonly || mount-params->is_readonly
    if (fscIsReadOnly || (vhp->attributes & kSFSVolAttrib_ReadOnly) == kSFSVolAttrib_ReadOnly) {
        pOutProps->isReadOnly = true;
    }
    
#ifdef __SERENA__
    // XXX disabled updates to the access dates for now as long as there's no
    // XXX disk cache and no boot-from-HD support
    self->mountFlags.isAccessUpdateOnReadEnabled = 0;
#endif

catch:
    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_None);

    return err;
}

errno_t SerenaFS_onStop(SerenaFSRef _Nonnull self)
{
    // XXX flush all still cached file data to disk (synchronously)

    SfsAllocator_CommitToDisk(&self->blockAllocator, Filesystem_GetContainer(self));
    SfsAllocator_Stop(&self->blockAllocator);

    return EOK;
}

static errno_t SerenaFS_unlinkCore(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked dir)
{
    decl_try_err();

    // Remove the directory entry in the parent directory
    try(SfsDirectory_RemoveEntry(dir, pNodeToUnlink));
    Inode_Writeback(dir);
    SfsAllocator_CommitToDisk(&self->blockAllocator, Filesystem_GetContainer(self));


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);
    Inode_SetModified(pNodeToUnlink, kInodeFlag_StatusChanged);

catch:
    return err;
}

// Unlink the node 'target' which is an immediate child of 'dir'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t SerenaFS_unlink(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked target, InodeRef _Nonnull _Locked dir)
{
    decl_try_err();

    // A directory must be empty in order to be allowed to unlink its
    if (Inode_IsDirectory(target) && Inode_GetLinkCount(target) > 1 && SfsDirectory_IsNotEmpty(target)) {
        throw(EBUSY);
    }


    try(SerenaFS_unlinkCore(self, target, dir));

catch:
    return err;
}

errno_t SerenaFS_link(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull name, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();

    try(SfsDirectory_CanAcceptEntry(pDstDir, name, Inode_GetFileType(pSrcNode)));
    try(SfsDirectory_InsertEntry(pDstDir, name, pSrcNode, (sfs_insertion_hint_t*)pDirInstHint->data));
    Inode_Writeback(pDstDir);

    Inode_Link(pSrcNode);
    Inode_SetModified(pSrcNode, kInodeFlag_StatusChanged);

catch:
    return err;
}

errno_t SerenaFS_move(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, InodeRef _Nonnull _Locked pSrcDir, InodeRef _Nonnull _Locked pDstDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid, const DirectoryEntryInsertionHint* _Nonnull pDirInstHint)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const bool isMovingDir = Inode_IsDirectory(pNode);

    // The 'moveLock' ensures that there can be only one operation active at any
    // given time that might move directories around in the filesystem. This ie
    // ensures that the result that we get from calling IsAscendentOfDirectory()
    // stays meaningful while we are busy executing the move.
    Lock_Lock(&self->moveLock);

    if (isMovingDir && SfsDirectory_IsAncestorOf(pNode, pSrcDir, pDstDir)) {
        // oldpath is an ancestor of newpath (Don't allow moving a directory inside of itself)
        throw(EINVAL);
    }


    // Add a new entry in the destination directory and remove the old entry from
    // the source directory
    try(SerenaFS_link(self, pNode, pDstDir, pNewName, uid, gid, pDirInstHint));
    try(SerenaFS_unlinkCore(self, pNode, pSrcDir));  // XXX should theoretically be able to use unlink() here. Fails with resource busy because we trigger the empty check on the destination directory


    // Re-point the moved inode to its new parent node
    Inode_SetParentId(pNode, Inode_GetId(pDstDir));


    // If we're moving a directory then we need to re-point its parent entry '..'
    // to the new parent directory
    if (isMovingDir) {
        try(SfsDirectory_UpdateParentEntry(pNode, Inode_GetId(pDstDir)));
        
        // Our parent receives a +1 on the link count because of our .. entry
        Inode_Link(pDstDir);
    }

catch:
    Lock_Unlock(&self->moveLock);
    return err;
}

errno_t SerenaFS_rename(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pSrcNode, InodeRef _Nonnull _Locked pSrcDir, const PathComponent* _Nonnull pNewName, uid_t uid, gid_t gid)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    sfs_query_t q;
    sfs_query_result_t qr;
    FSBlock blk = {0};

    if (pNewName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }
    
    q.kind = kSFSQuery_InodeId;
    q.u.id = Inode_GetId(pSrcNode);
    q.mpc = NULL;
    q.ih = NULL;
    try(SfsDirectory_Query(pSrcDir, &q, &qr));

    try(FSContainer_MapBlock(fsContainer, qr.lba, kMapBlock_Update, &blk));

    sfs_dirent_t* dep = (sfs_dirent_t*)(blk.data + qr.blockOffset);
    memset(dep->filename, 0, kSFSMaxFilenameLength);
    memcpy(dep->filename, pNewName->name, pNewName->count);
    dep->len = pNewName->count;

    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);

catch:
    return err;
}


class_func_defs(SerenaFS, ContainerFilesystem,
override_func_def(deinit, SerenaFS, Object)
override_func_def(onAcquireNode, SerenaFS, Filesystem)
override_func_def(onWritebackNode, SerenaFS, Filesystem)
override_func_def(onStart, SerenaFS, Filesystem)
override_func_def(onStop, SerenaFS, Filesystem)
override_func_def(acquireNodeForName, SerenaFS, Filesystem)
override_func_def(getNameOfNode, SerenaFS, Filesystem)
override_func_def(createNode, SerenaFS, Filesystem)
override_func_def(unlink, SerenaFS, Filesystem)
override_func_def(move, SerenaFS, Filesystem)
override_func_def(rename, SerenaFS, Filesystem)
);
