//
//  SerenaFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode extensions
////////////////////////////////////////////////////////////////////////////////

// Returns true if the given directory node is not empty (contains more than
// just "." and ".." or has a link count > 1).
static bool DirectoryNode_IsNotEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetLinkCount(self) > 1 || Inode_GetFileSize(self) > 2 * sizeof(SFSDirectoryEntry);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Allocation Bitmaps
////////////////////////////////////////////////////////////////////////////////

// Returns true if the allocation block 'lba' is in use and false otherwise
static bool AllocationBitmap_IsBlockInUse(const uint8_t *bitmap, LogicalBlockAddress lba)
{
    return ((bitmap[lba >> 3] & (1 << (7 - (lba & 0x07)))) != 0) ? true : false;
}

// Sets the in-use bit corresponding to the logical block address 'lba' as in-use or not
static void AllocationBitmap_SetBlockInUse(uint8_t *bitmap, LogicalBlockAddress lba, bool inUse)
{
    uint8_t* bytePtr = &bitmap[lba >> 3];
    const uint8_t bitNo = 7 - (lba & 0x07);

    if (inUse) {
        *bytePtr |= (1 << bitNo);
    }
    else {
        *bytePtr &= ~(1 << bitNo);
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

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
    ip->blockMap.p[0] = UInt32_HostToBig(rootDirContentLba);
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
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->notifier);
    self->isReadOnly = false;

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

void SerenaFS_deinit(SerenaFSRef _Nonnull self)
{
#ifndef __DISKIMAGE__
    // Can not be that we are getting deallocated while being mounted
    // diskimage note: the diskimage tool isn't currently properly unmounting
    // the FS which would trigger this assert. Disabled it for now
    assert(self->diskDriver == NULL);
#endif
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static errno_t SerenaFS_WriteBackAllocationBitmapForLba(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    const LogicalBlockAddress idxOfAllocBitmapBlockModified = (lba >> 3) / kSFSBlockSize;
    const uint8_t* pBlock = &self->allocationBitmap[idxOfAllocBitmapBlockModified * kSFSBlockSize];
    const LogicalBlockAddress allocationBitmapBlockLba = self->allocationBitmapLba + idxOfAllocBitmapBlockModified;

    memset(self->tmpBlock, 0, kSFSBlockSize);
    memcpy(self->tmpBlock, pBlock, &self->allocationBitmap[self->allocationBitmapByteSize] - pBlock);
    return DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, allocationBitmapBlockLba);
}

static errno_t SerenaFS_AllocateBlock_Locked(SerenaFSRef _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba)
{
    decl_try_err();
    LogicalBlockAddress lba = 0;    // Safe because LBA #0 is the volume header which is always allocated when the FS is mounted

    for (LogicalBlockAddress i = 1; i < self->volumeBlockCount; i++) {
        if (!AllocationBitmap_IsBlockInUse(self->allocationBitmap, i)) {
            lba = i;
            break;
        }
    }
    if (lba == 0) {
        throw(ENOSPC);
    }

    AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, true);
    try(SerenaFS_WriteBackAllocationBitmapForLba(self, lba));

    *pOutLba = lba;
    return EOK;

catch:
    if (lba > 0) {
        AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);
    }
    *pOutLba = 0;
    return err;
}

static void SerenaFS_DeallocateBlock_Locked(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    if (lba == 0) {
        return;
    }

    AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);

    // XXX check for error here?
    SerenaFS_WriteBackAllocationBitmapForLba(self, lba);
}

// Creates a new inode with type 'type', user information 'user', permissions
// 'permissions' and adds it to parent inode (directory) 'pParentNode'. The new
// node will be added to 'pParentNode' with the name 'pName'. Returns the newly
// acquired inode on success and NULL otherwise.
static errno_t SerenaFS_CreateNode(SerenaFSRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull pParentNode, const PathComponent* _Nonnull pName, SFSDirectoryEntryPointer* _Nullable pDirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    InodeId parentInodeId = Inode_GetId(pParentNode);
    LogicalBlockAddress inodeLba = 0;
    LogicalBlockAddress dirContLba = 0;
    FileOffset fileSize = 0ll;
    SFSBlockMap* pBlockMap = NULL;
    InodeRef pNode = NULL;
    bool isPublished = false;

    try(kalloc_cleared(sizeof(SFSBlockMap), (void**)&pBlockMap));
    try(SerenaFS_AllocateBlock_Locked(self, &inodeLba));
    
    if (type == kFileType_Directory) {
        // Write the initial directory content. This are just the '.' and '..'
        // entries
        try(SerenaFS_AllocateBlock_Locked(self, &dirContLba));

        memset(self->tmpBlock, 0, kSFSBlockSize);
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)self->tmpBlock;
        dep[0].id = UInt32_HostToBig(inodeLba);
        dep[0].filename[0] = '.';
        dep[1].id = UInt32_HostToBig(parentInodeId);
        dep[1].filename[0] = '.';
        dep[1].filename[1] = '.';
        try(DiskDriver_PutBlock(self->diskDriver, dep, dirContLba));

        pBlockMap->p[0] = dirContLba;
        fileSize = 2 * sizeof(SFSDirectoryEntry);
    }

    try(Inode_Create(
        Filesystem_GetId(self),
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
        pBlockMap,
        &pNode));
    Inode_SetModified(pNode, kInodeFlag_Accessed | kInodeFlag_Updated | kInodeFlag_StatusChanged);


    // Be sure to publish the newly create inode before we add it to the parent
    // directory. This ensures that a guy who stumbles across the new directory
    // entry and calls Filesystem_AcquireNode() on it, won't unexpectedly create
    // a second inode object representing the same inode.  
    try(Filesystem_PublishNode((FilesystemRef)self, pNode));
    isPublished = true;
    try(SerenaFS_InsertDirectoryEntry(self, pParentNode, pName, Inode_GetId(pNode), pDirInsertionHint));
    *pOutNode = pNode;

    return EOK;

catch:
    if (isPublished) {
        Filesystem_Unlink((FilesystemRef)self, pNode, pParentNode, user);
        Filesystem_RelinquishNode((FilesystemRef)self, pNode);
    } else {
        Inode_Destroy(pNode);
    }
    kfree(pBlockMap);

    if (dirContLba != 0) {
        SerenaFS_DeallocateBlock_Locked(self, dirContLba);
    }
    if (inodeLba != 0) {
        SerenaFS_DeallocateBlock_Locked(self, inodeLba);
    }
    *pOutNode = NULL;

    return err;
}

// Invoked when Filesystem_AcquireNodeWithId() needs to read the requested inode
// off the disk. The override should read the inode data from the disk,
// create and inode instance and fill it in with the data from the disk and
// then return it. It should return a suitable error and NULL if the inode
// data can not be read off the disk.
errno_t SerenaFS_onReadNodeFromDisk(SerenaFSRef _Nonnull self, InodeId id, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const LogicalBlockAddress lba = (LogicalBlockAddress)id;
    SFSBlockMap* pBlockMap = NULL;

    try(kalloc(sizeof(SFSBlockMap), (void**)&pBlockMap));
    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
    const SFSInode* ip = (const SFSInode*)self->tmpBlock;

    for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
        pBlockMap->p[i] = UInt32_BigToHost(ip->blockMap.p[i]);
    }

    return Inode_Create(
        Filesystem_GetId(self),
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
        pBlockMap,
        pOutNode);

catch:
    kfree(pBlockMap);
    *pOutNode = NULL;
    return err;
}

// Invoked when the inode is relinquished and it is marked as modified. The
// filesystem override should write the inode meta-data back to the 
// corresponding disk node.
errno_t SerenaFS_onWriteNodeToDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);
    const SFSBlockMap* pBlockMap = (const SFSBlockMap*)Inode_GetBlockMap(pNode);
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    SFSInode* ip = (SFSInode*)self->tmpBlock;

    memset(ip, 0, kSFSBlockSize);

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

    for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
        ip->blockMap.p[i] = UInt32_HostToBig(pBlockMap->p[i]);
    }

    return DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, lba);
}

static void SerenaFS_DeallocateFileContentBlocks_Locked(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const SFSBlockMap* pBlockMap = (const SFSBlockMap*)Inode_GetBlockMap(pNode);

    for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
        if (pBlockMap->p[i] == 0) {
            break;
        }

        SerenaFS_DeallocateBlock_Locked(self, pBlockMap->p[i]);
    }
}

// Invoked when Filesystem_RelinquishNode() has determined that the inode is
// no longer being referenced by any directory and that the on-disk
// representation should be deleted from the disk and deallocated. This
// operation is assumed to never fail.
void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);

    SerenaFS_DeallocateFileContentBlocks_Locked(self, pNode);
    SerenaFS_DeallocateBlock_Locked(self, lba);
}

// Checks whether the given user should be granted access to the given node based
// on the requested permission. Returns EOK if access should be granted and a suitable
// error code if it should be denied.
static errno_t SerenaFS_CheckAccess_Locked(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, AccessMode mode)
{
    if (mode == kFilePermission_Write) {
        if (self->isReadOnly) {
            return EROFS;
        }

        // XXX once we support actual text mapping, we'll need to check whether the text file is in use
    }

    FilePermissions permissions;
    switch (mode) {
        case kAccess_Readable:      permissions = FilePermissions_Make(kFilePermission_Read, 0, 0); break;
        case kAccess_Writable:      permissions = FilePermissions_Make(kFilePermission_Write, 0, 0); break;
        case kAccess_Executable:    permissions = FilePermissions_Make(kFilePermission_Execute, 0, 0); break;
        default:                    permissions = 0; break;
    }
    return Inode_CheckAccess(pNode, user, permissions);
}

// Returns true if the array of directory entries starting at 'pEntry' and holding
// 'nEntries' entries contains a directory entry that matches 'pQuery'.
static bool xHasMatchingDirectoryEntry(const SFSDirectoryQuery* _Nonnull pQuery, const SFSDirectoryEntry* _Nonnull pBlock, int nEntries, SFSDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, SFSDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    const SFSDirectoryEntry* pEntry = pBlock;

    *pOutEmptyPtr = NULL;
    *pOutEntryPtr = NULL;

    while (nEntries-- > 0) {
        if (pEntry->id > 0) {
            switch (pQuery->kind) {
                case kSFSDirectoryQuery_PathComponent:
                    if (PathComponent_EqualsString(pQuery->u.pc, pEntry->filename)) {
                        *pOutEntryPtr = (SFSDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                case kSFSDirectoryQuery_InodeId:
                    if (pEntry->id == pQuery->u.id) {
                       *pOutEntryPtr = (SFSDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                default:
                    abort();
            }
        }
        else if (pOutEmptyPtr) {
            *pOutEmptyPtr = (SFSDirectoryEntry*)pEntry;
        }
        pEntry++;
    }

    return false;
}

// Returns a reference to the directory entry that holds 'pName'. NULL and a
// suitable error is returned if no such entry exists or 'pName' is empty or
// too long.
static errno_t SerenaFS_GetDirectoryEntry(
    SerenaFSRef _Nonnull self,
    InodeRef _Nonnull _Locked pNode,
    const SFSDirectoryQuery* _Nonnull pQuery,
    SFSDirectoryEntryPointer* _Nullable pOutEmptyPtr,
    SFSDirectoryEntryPointer* _Nullable pOutEntryPtr,
    InodeId* _Nullable pOutId,
    MutablePathComponent* _Nullable pOutFilename)
{
    decl_try_err();
    const SFSDirectoryEntry* pDirBuffer = (const SFSDirectoryEntry*)self->tmpBlock;
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    FileOffset offset = 0ll;
    LogicalBlockAddress lba = 0;
    SFSDirectoryEntry* pEmptyEntry = NULL;
    SFSDirectoryEntry* pMatchingEntry = NULL;
    SFSDirectoryQuery swappedQuery;
    bool hasMatch = false;

    if (pOutEmptyPtr) {
        pOutEmptyPtr->lba = 0;
        pOutEmptyPtr->offset = 0;
        pOutEmptyPtr->fileOffset = 0ll;
    }
    if (pOutEntryPtr) {
        pOutEntryPtr->lba = 0;
        pOutEntryPtr->offset = 0;
        pOutEntryPtr->fileOffset = 0ll;
    }
    if (pOutId) {
        *pOutId = 0;
    }
    if (pOutFilename) {
        pOutFilename->count = 0;
    }

    if (pQuery->kind == kSFSDirectoryQuery_PathComponent) {
        if (pQuery->u.pc->count == 0) {
            return ENOENT;
        }
        if (pQuery->u.pc->count > kSFSMaxFilenameLength) {
            return ENAMETOOLONG;
        }
    }

    swappedQuery = *pQuery;
    if (swappedQuery.kind == kSFSDirectoryQuery_InodeId) {
        swappedQuery.u.id = UInt32_HostToBig(swappedQuery.u.id);
    }

    while (true) {
        const int blockIdx = offset >> (FileOffset)kSFSBlockSizeShift;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)kSFSBlockSize, fileSize - offset);

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Read, &lba));
        if (lba == 0) {
            memset(self->tmpBlock, 0, kSFSBlockSize);
        }
        else {
            try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
        }

        const int nDirEntries = nBytesAvailable / sizeof(SFSDirectoryEntry);
        hasMatch = xHasMatchingDirectoryEntry(&swappedQuery, pDirBuffer, nDirEntries, &pEmptyEntry, &pMatchingEntry);
        if (pEmptyEntry) {
            pOutEmptyPtr->lba = lba;
            pOutEmptyPtr->offset = ((uint8_t*)pEmptyEntry) - ((uint8_t*)pDirBuffer);
            pOutEmptyPtr->fileOffset = offset + pOutEmptyPtr->offset;
        }
        if (hasMatch) {
            break;
        }

        offset += (FileOffset)nBytesAvailable;
    }

    if (hasMatch) {
        if (pOutEntryPtr) {
            pOutEntryPtr->lba = lba;
            pOutEntryPtr->offset = ((uint8_t*)pMatchingEntry) - ((uint8_t*)pDirBuffer);
            pOutEntryPtr->fileOffset = offset + pOutEntryPtr->offset;
        }
        if (pOutId) {
            *pOutId = UInt32_BigToHost(pMatchingEntry->id);
        }
        if (pOutFilename) {
            const ssize_t len = String_LengthUpTo(pMatchingEntry->filename, kSFSMaxFilenameLength);
            if (len > pOutFilename->capacity) {
                throw(ERANGE);
            }

            String_CopyUpTo(pOutFilename->name, pMatchingEntry->filename, len);
            pOutFilename->count = len;
        }
        return EOK;
    }
    else {
        return ENOENT;
    }

catch:
    return err;
}

// Looks up the absolute logical block address for the disk block that corresponds
// to the file-specific logical block address 'fba'.
// The first logical block is #0 at the very beginning of the file 'pNode'. Logical
// block addresses increment by one until the end of the file. Note that not every
// logical block address may be backed by an actual disk block. A missing disk block
// must be substituted by an empty block. 0 is returned if no absolute logical
// block address exists for 'fba'.
// XXX 'fba' should be LogicalBlockAddress. However we want to be able to detect overflows
static errno_t SerenaFS_GetLogicalBlockAddressForFileBlockAddress(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, int fba, SFSBlockMode mode, LogicalBlockAddress* _Nonnull pOutLba)
{
    decl_try_err();

    if (fba < 0 || fba >= kSFSMaxDirectDataBlockPointers) {
        throw(EFBIG);
    }

    SFSBlockMap* pBlockMap = Inode_GetBlockMap(pNode);
    LogicalBlockAddress lba = pBlockMap->p[fba];

    if (lba == 0 && mode == kSFSBlockMode_Write) {
        // XXX fix locking here
        try(SerenaFS_AllocateBlock_Locked(self, &lba));
        pBlockMap->p[fba] = lba;
    }
    *pOutLba = lba;
    return EOK;

catch:
    *pOutLba = 0;
    return err;
}

// Reads 'nBytesToRead' bytes from the file 'pNode' starting at offset 'offset'.
static errno_t SerenaFS_xRead(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    ssize_t nBytesRead = 0;

    if (nBytesToRead > 0 && (offset < 0ll || offset > kSFSLimit_FileSizeMax)) {
        *pOutBytesRead = 0;
        return EOVERFLOW;
    }
    
    while (nBytesToRead > 0 && offset < fileSize) {
        const int blockIdx = (int)(offset >> (FileOffset)kSFSBlockSizeShift);   //XXX blockIdx should be 64bit
        const size_t blockOffset = offset & (FileOffset)kSFSBlockSizeMask;
        const size_t nBytesToReadInCurrentBlock = (size_t)__min((FileOffset)(kSFSBlockSize - blockOffset), __min(fileSize - offset, (FileOffset)nBytesToRead));
        LogicalBlockAddress lba;

        errno_t e1 = SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Read, &lba);
        if (e1 == EOK) {
            if (lba == 0) {
                memset(self->tmpBlock, 0, kSFSBlockSize);
            }
            else {
                e1 = DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba);
            }
        }
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }

        memcpy(((uint8_t*)pBuffer) + nBytesRead, self->tmpBlock + blockOffset, nBytesToReadInCurrentBlock);
        nBytesToRead -= nBytesToReadInCurrentBlock;
        nBytesRead += nBytesToReadInCurrentBlock;
        offset += (FileOffset)nBytesToReadInCurrentBlock;
    }

    *pOutBytesRead = nBytesRead;
    if (*pOutBytesRead > 0) {
        Inode_SetModified(pNode, kInodeFlag_Accessed);
    }
    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'pNode' starting at offset 'offset'.
static errno_t SerenaFS_xWrite(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    ssize_t nBytesWritten = 0;

    if (nBytesToWrite > 0 && (offset < 0ll || offset > kSFSLimit_FileSizeMax)) {
        *pOutBytesWritten = 0;
        return EOVERFLOW;
    }

    while (nBytesToWrite > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kSFSBlockSizeShift);   //XXX blockIdx should be 64bit
        const size_t blockOffset = offset & (FileOffset)kSFSBlockSizeMask;
        const size_t nBytesToWriteInCurrentBlock = __min(kSFSBlockSize - blockOffset, nBytesToWrite);
        LogicalBlockAddress lba;

        errno_t e1 = SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Write, &lba);
        if (e1 == EOK) {
            if (lba == 0) {
                memset(self->tmpBlock, 0, kSFSBlockSize);
            }
            else {
                e1 = DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba);
            }
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(self->tmpBlock + blockOffset, ((const uint8_t*) pBuffer) + nBytesWritten, nBytesToWriteInCurrentBlock);
        e1 = DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, lba);
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesToWrite -= nBytesToWriteInCurrentBlock;
        nBytesWritten += nBytesToWriteInCurrentBlock;
        offset += (FileOffset)nBytesToWriteInCurrentBlock;
    }

    if (nBytesWritten > 0) {
        if (offset > Inode_GetFileSize(pNode)) {
            Inode_SetFileSize(pNode, offset);
        }
        Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }
    *pOutBytesWritten = nBytesWritten;
    return err;
}


// Invoked when an instance of this file system is mounted. Note that the
// kernel guarantees that no operations will be issued to the filesystem
// before onMount() has returned with EOK.
errno_t SerenaFS_onMount(SerenaFSRef _Nonnull self, DiskDriverRef _Nonnull pDriver, const void* _Nonnull pParams, ssize_t paramsSize)
{
    decl_try_err();

    Lock_Lock(&self->lock);

    if (self->diskDriver) {
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
    
catch:
    Lock_Unlock(&self->lock);
    return err;
}

// Invoked when a mounted instance of this file system is unmounted. A file
// system may return an error. Note however that this error is purely advisory
// and the file system implementation is required to do everything it can to
// successfully unmount. Unmount errors are ignored and the file system manager
// will complete the unmount in any case.
errno_t SerenaFS_onUnmount(SerenaFSRef _Nonnull self)
{
    decl_try_err();

    Lock_Lock(&self->lock);
    if (self->diskDriver == NULL) {
        throw(EIO);
    }

    // XXX wait for still ongoing FS operations to settle

    // XXX make sure that there are no inodes in use anymore

    // XXX flush all still cached file data to disk (synchronously)

    // XXX flush the allocation bitmap to disk (synchronously)
    // XXX free the allocation bitmap and clear self->volumeBlockCount

    // XXX clear rootDirLba
    
    Object_Release(self->diskDriver);
    self->diskDriver = NULL;

catch:
    Lock_Unlock(&self->lock);
/*
    Lock_Lock(&self->lock);
    if (!self->isMounted) {
        throw(EIO);
    }

    // There might be one or more operations currently ongoing. Wait until they
    // are done.
    while(self->busyCount > 0) {
        try(ConditionVariable_Wait(&self->notifier, &self->lock, kTimeInterval_Infinity));
    }


    // Make sure that there are no open files anywhere referencing us
    if (!FilesystemManager_CanSafelyUnmountFilesystem(gFilesystemManager, (FilesystemRef)self)) {
        throw(EBUSY);
    }

    // XXX Flush dirty buffers to disk

    Object_Release(self->root);
    self->root = NULL;

catch:
    Lock_Unlock(&self->lock);
    */
    return err;
}


// Returns the root node of the filesystem if the filesystem is currently in
// mounted state. Returns ENOENT and NULL if the filesystem is not mounted.
errno_t SerenaFS_acquireRootNode(SerenaFSRef _Nonnull self, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    return Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirLba, NULL, pOutNode);
}

// Returns EOK and the node that corresponds to the tuple (parent-node, name),
// if that node exists. Otherwise returns ENOENT and NULL.  Note that this
// function has to support the special names "." (node itself) and ".."
// (parent of node) in addition to "regular" filenames. If 'pParentNode' is
// the root node of the filesystem and 'pComponent' is ".." then 'pParentNode'
// should be returned. If the path component name is longer than what is
// supported by the file system, ENAMETOOLONG should be returned.
errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pName, User user, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();
    SFSDirectoryQuery q;
    InodeId entryId;

    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Execute));
    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    try(SerenaFS_GetDirectoryEntry(self, pParentNode, &q, NULL, NULL, &entryId, NULL));
    try(Filesystem_AcquireNodeWithId((FilesystemRef)self, entryId, NULL, pOutNode));
    return EOK;

catch:
    *pOutNode = NULL;
    return err;
}

// Returns the name of the node with the id 'id' which a child of the
// directory node 'pParentNode'. 'id' may be of any type. The name is
// returned in the mutable path component 'pComponent'. 'count' in path
// component is 0 on entry and should be set to the actual length of the
// name on exit. The function is expected to return EOK if the parent node
// contains 'id' and ENOENT otherwise. If the name of 'id' as stored in the
// file system is > the capacity of the path component, then ERANGE should
// be returned.
errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pParentNode, InodeId id, User user, MutablePathComponent* _Nonnull pComponent)
{
    decl_try_err();
    SFSDirectoryQuery q;

    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Read | kFilePermission_Execute));
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = id;
    try(SerenaFS_GetDirectoryEntry(self, pParentNode, &q, NULL, NULL, NULL, pComponent));
    return EOK;

catch:
    pComponent->count = 0;
    return err;
}

// Returns a file info record for the given Inode. The Inode may be of any
// file type.
errno_t SerenaFS_getFileInfo(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileInfo* _Nonnull pOutInfo)
{
    Inode_GetFileInfo(pNode, pOutInfo);
    return EOK;
}

// Modifies one or more attributes stored in the file info record of the given
// Inode. The Inode may be of any type.
errno_t SerenaFS_setFileInfo(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, MutableFileInfo* _Nonnull pInfo)
{
    decl_try_err();

    if (self->isReadOnly) {
        throw(EROFS);
    }
    try(Inode_SetFileInfo(pNode, user, pInfo));

catch:
    return err;
}

static errno_t SerenaFS_RemoveDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, InodeId idToRemove)
{
    decl_try_err();
    SFSDirectoryEntryPointer mp;
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = idToRemove;
    try(SerenaFS_GetDirectoryEntry(self, pDirNode, &q, NULL, &mp, NULL, NULL));

    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, mp.lba));
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + mp.offset);
    memset(dep, 0, sizeof(SFSDirectoryEntry));
    try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, mp.lba));

    if (Inode_GetFileSize(pDirNode) - (FileOffset)sizeof(SFSDirectoryEntry) == mp.fileOffset) {
        Inode_DecrementFileSize(pDirNode, sizeof(SFSDirectoryEntry));
    }

    return EOK;

catch:
    return err;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'pDirNode'. 'pEmptyEntry' is an optional insertion hint. If this pointer exists
// then the directory entry that it points to will be reused for the new directory
// entry; otherwise a completely new entry will be added to the directory.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
static errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, SFSDirectoryEntryPointer* _Nullable pEmptyPtr)
{
    decl_try_err();

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyPtr && pEmptyPtr->lba > 0) {
        // Reuse an empty entry
        try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, pEmptyPtr->lba));
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + pEmptyPtr->offset);

        char* p = String_CopyUpTo(dep->filename, pName->name, pName->count);
        while (p < &dep->filename[kSFSMaxFilenameLength]) *p++ = '\0';
        dep->id = UInt32_HostToBig(id);

        try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, pEmptyPtr->lba));
    }
    else {
        // Append a new entry
        SFSBlockMap* pBlockMap = Inode_GetBlockMap(pDirNode);
        const FileOffset size = Inode_GetFileSize(pDirNode);
        const int remainder = size & kSFSBlockSizeMask;
        SFSDirectoryEntry* dep;
        LogicalBlockAddress lba;
        int idx = -1;

        if (remainder > 0) {
            idx = size / kSFSBlockSize;
            lba = pBlockMap->p[idx];

            try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
            dep = (SFSDirectoryEntry*)(self->tmpBlock + remainder);
        }
        else {
            for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
                if (pBlockMap->p[i] == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) {
                throw(EIO);
            }

            try(SerenaFS_AllocateBlock_Locked(self, &lba));
            memset(self->tmpBlock, 0, kSFSBlockSize);
            dep = (SFSDirectoryEntry*)self->tmpBlock;
        }

        String_CopyUpTo(dep->filename, pName->name, pName->count);
        dep->id = UInt32_HostToBig(id);
        try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, lba));
        pBlockMap->p[idx] = lba;

        Inode_IncrementFileSize(pDirNode, sizeof(SFSDirectoryEntry));
    }


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDirNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
errno_t SerenaFS_createDirectory(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();
    InodeRef pDirNode = NULL;

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // Make sure that the parent directory is able to accept one more link
    if (Inode_GetLinkCount(pParentNode) >= kSFSLimit_LinkMax) {
        throw(EMLINK);
    }


    // Make sure that 'pParentNode' doesn't already have an entry with name 'pName'.
    // Also figure out whether there's an empty entry that we can reuse.
    SFSDirectoryEntryPointer ep;
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SerenaFS_GetDirectoryEntry(self, pParentNode, &q, &ep, NULL, NULL, NULL);
    if (err == ENOENT) {
        err = EOK;
    } else if (err == EOK) {
        throw(EEXIST);
    } else {
        throw(err);
    }


    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Create the new directory and add it to its parent directory
    try(SerenaFS_CreateNode(self, kFileType_Directory, user, permissions, pParentNode, pName, &ep, &pDirNode));
    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);


    // Increment the parent directory link count to account for the '..' entry
    // in the just created subdirectory
    Inode_Link(pParentNode);

catch:
    return err;
}

// Opens the directory represented by the given node. The filesystem is
// expected to validate whether the user has access to the directory content.
errno_t SerenaFS_openDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user)
{
    return Inode_CheckAccess(pDirNode, user, kFilePermission_Read);
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull pDirNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    FileOffset offset = *pInOutOffset;
    SFSDirectoryEntry dirent;
    ssize_t nAllDirBytesRead = 0;
    ssize_t nBytesRead = 0;

    while (nBytesToRead > 0) {
        ssize_t nDirBytesRead;
        const errno_t e1 = SerenaFS_xRead(self, pDirNode, offset, &dirent, sizeof(SFSDirectoryEntry), &nDirBytesRead);

        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        else if (nDirBytesRead == 0) {
            break;
        }

        if (dirent.id > 0) {
            DirectoryEntry* pEntry = (DirectoryEntry*)((uint8_t*)pBuffer + nBytesRead);

            if (nBytesToRead < sizeof(DirectoryEntry)) {
                break;
            }

            pEntry->inodeId = UInt32_BigToHost(dirent.id);
            String_CopyUpTo(pEntry->name, dirent.filename, kSFSMaxFilenameLength);
            nBytesRead += sizeof(DirectoryEntry);
            nBytesToRead -= sizeof(DirectoryEntry);
        }

        offset += nDirBytesRead;
        nAllDirBytesRead += nDirBytesRead;
    }

    if (nBytesRead > 0) {
        *pInOutOffset += nAllDirBytesRead;
    }
    *nOutBytesRead = nBytesRead;

    return err;
}

// Opens the file identified by the given inode. The file is opened for reading
// and or writing, depending on the 'mode' bits.
errno_t SerenaFS_openFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user)
{
    decl_try_err();
    FilePermissions permissions = 0;

    if (Inode_IsDirectory(pNode)) {
        throw(EISDIR);
    }

    if ((mode & kOpen_ReadWrite) == 0) {
        throw(EACCESS);
    }
    if ((mode & kOpen_Read) == kOpen_Read) {
        permissions |= kFilePermission_Read;
    }
    if ((mode & kOpen_Write) == kOpen_Write || (mode & kOpen_Truncate) == kOpen_Truncate) {
        permissions |= kFilePermission_Write;
    }

    try(Inode_CheckAccess(pNode, user, permissions));


    // A negative file size is treated as an overflow
    if (Inode_GetFileSize(pNode) < 0ll || Inode_GetFileSize(pNode) > kSFSLimit_FileSizeMax) {
        throw(EOVERFLOW);
    }


    if ((mode & kOpen_Truncate) == kOpen_Truncate) {
        SerenaFS_xTruncateFile(self, pNode, 0);
    }
    
catch:
    return err;
}

// Creates and opens a file and returns the inode of that file. The behavior is
// non-exclusive by default. Meaning the file is created if it does not 
// exist and the file's inode is merrily acquired if it already exists. If
// the mode is exclusive then the file is created if it doesn't exist and
// an error is thrown if the file exists. Returns a file object, representing
// the created and opened file.
errno_t SerenaFS_createFile(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, unsigned int mode, FilePermissions permissions, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    InodeRef pInode = NULL;

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // Check whether a file with name 'pName' already exists
    InodeId existingFileId;
    SFSDirectoryEntryPointer ep;
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SerenaFS_GetDirectoryEntry(self, pParentNode, &q, &ep, NULL, &existingFileId, NULL);
    if (err == ENOENT) {
        // File does not exist - create it
        err = EOK;


        // We must have write permissions for 'pParentNode'
        try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


        // The user provided read/write mode must match up with the provided (user) permissions
        if ((mode & kOpen_ReadWrite) == 0) {
            throw(EACCESS);
        }
        if ((mode & kOpen_Read) == kOpen_Read && !FilePermissions_Has(permissions, kFilePermissionsScope_User, kFilePermission_Read)) {
            throw(EACCESS);
        }
        if ((mode & kOpen_Write) == kOpen_Write && !FilePermissions_Has(permissions, kFilePermissionsScope_User, kFilePermission_Write)) {
            throw(EACCESS);
        }


        // Create the new file and add it to its parent directory
        try(SerenaFS_CreateNode(self, kFileType_RegularFile, user, permissions, pParentNode, pName, &ep, &pInode));
        *pOutNode = pInode;
    }
    else if (err == EOK) {
        // File exists - reject the operation in exclusive mode and open the file
        // in non-exclusive mode
        if ((mode & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }


        try(Filesystem_AcquireNodeWithId((FilesystemRef)self, existingFileId, NULL, &pInode));
        try(SerenaFS_openFile(self, pInode, mode, user));
        *pOutNode = pInode;
    }
    else {
        // Some lookup error
        throw(err);
    }

    return EOK;

catch:
    if (pInode) {
        Filesystem_RelinquishNode((FilesystemRef)self, pInode);
    }
    *pOutNode = NULL;
    
    return err;
}

errno_t SerenaFS_readFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    const errno_t err = SerenaFS_xRead(self, 
        pNode, 
        *pInOutOffset,
        pBuffer,
        nBytesToRead,
        nOutBytesRead);
    *pInOutOffset += *nOutBytesRead;
    return err;
}

errno_t SerenaFS_writeFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesWritten)
{
    const errno_t err = SerenaFS_xWrite(self, 
        pNode, 
        *pInOutOffset,
        pBuffer,
        nBytesToWrite,
        nOutBytesWritten);
    *pInOutOffset += *nOutBytesWritten;
    return err;
}

// Internal file truncation function. Shortens the file 'pNode' to the new and
// smaller size 'length'. Does not support increasing the size of a file.
static void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset length)
{
    const FileOffset oldLength = Inode_GetFileSize(pNode);
    const FileOffset oldLengthRoundedUpToBlockBoundary = __Ceil_PowerOf2(oldLength, kSFSBlockSize);
    const int firstBlockIdx = (int)(oldLengthRoundedUpToBlockBoundary >> (FileOffset)kSFSBlockSizeShift);    //XXX blockIdx should be 64bit
    SFSBlockMap* pBlockMap = Inode_GetBlockMap(pNode);

    for (int i = firstBlockIdx; i < kSFSMaxDirectDataBlockPointers; i++) {
        if (pBlockMap->p[i] != 0) {
            // XXX locking
            SerenaFS_DeallocateBlock_Locked(self, pBlockMap->p[i]);
            pBlockMap->p[i] = 0;
        }
    }

    Inode_SetFileSize(pNode, length);
    Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}

// Change the size of the file 'pNode' to 'length'. EINVAL is returned if
// the new length is negative. No longer needed blocks are deallocated if
// the new length is less than the old length and zero-fille blocks are
// allocated and assigned to the file if the new length is longer than the
// old length. Note that a filesystem implementation is free to defer the
// actual allocation of the new blocks until an attempt is made to read or
// write them.
errno_t SerenaFS_truncate(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, FileOffset length)
{
    decl_try_err();

    if (Inode_IsDirectory(pNode)) {
        throw(EISDIR);
    }
    if (!Inode_IsRegularFile(pNode)) {
        throw(ENOTDIR);
    }
    if (length < 0) {
        throw(EINVAL);
    }
    try(Inode_CheckAccess(pNode, user, kFilePermission_Write));

    const FileOffset oldLength = Inode_GetFileSize(pNode);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(pNode, length);
        Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SerenaFS_xTruncateFile(self, pNode, length);
    }

catch:
    return err;
}

// Verifies that the given node is accessible assuming the given access mode.
errno_t SerenaFS_checkAccess(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, User user, int mode)
{
    decl_try_err();

    if ((mode & kAccess_Readable) == kAccess_Readable) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Read);
    }
    if (err == EOK && ((mode & kAccess_Writable) == kAccess_Writable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Write);
    }
    if (err == EOK && ((mode & kAccess_Executable) == kAccess_Executable)) {
        err = Inode_CheckAccess(pNode, user, kFilePermission_Execute);
    }

    return err;
}

// Unlink the node 'pNode' which is an immediate child of 'pParentNode'.
// Both nodes are guaranteed to be members of the same filesystem. 'pNode'
// is guaranteed to exist and that it isn't a mountpoint and not the root
// node of the filesystem.
// This function must validate that that if 'pNode' is a directory, that the
// directory is empty (contains nothing except "." and "..").
errno_t SerenaFS_unlink(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNodeToUnlink, InodeRef _Nonnull _Locked pParentNode, User user)
{
    decl_try_err();

    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // A directory must be empty in order to be allowed to unlink it
    if (Inode_IsDirectory(pNodeToUnlink) && DirectoryNode_IsNotEmpty(pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Remove the directory entry in the parent directory
    try(SerenaFS_RemoveDirectoryEntry(self, pParentNode, Inode_GetId(pNodeToUnlink)));
    SerenaFS_xTruncateFile(self, pParentNode, Inode_GetFileSize(pParentNode));


    // If this is a directory then unlink it from its parent since we remove a
    // '..' entry that points to the parent
    Inode_Unlink(pParentNode);


    // Unlink the node itself
    Inode_Unlink(pNodeToUnlink);
    Inode_SetModified(pNodeToUnlink, kInodeFlag_StatusChanged);

catch:
    return err;
}

// Renames the node with name 'pName' and which is an immediate child of the
// node 'pParentNode' such that it becomes a child of 'pNewParentNode' with
// the name 'pNewName'. All nodes are guaranteed to be owned by the filesystem.
errno_t SerenaFS_rename(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, const PathComponent* _Nonnull pNewName, InodeRef _Nonnull _Locked pNewParentNode, User user)
{
    // XXX implement me
    return EACCESS;
}


CLASS_METHODS(SerenaFS, Filesystem,
OVERRIDE_METHOD_IMPL(deinit, SerenaFS, Object)
OVERRIDE_METHOD_IMPL(onReadNodeFromDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onWriteNodeToDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onRemoveNodeFromDisk, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onMount, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(onUnmount, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireRootNode, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(acquireNodeForName, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(getNameOfNode, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(getFileInfo, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(setFileInfo, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(createFile, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(openFile, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(readFile, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(writeFile, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(createDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(openDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(truncate, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(checkAccess, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(unlink, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(rename, SerenaFS, Filesystem)
);
