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

static errno_t SerenaFS_WriteBackAllocationBitmapForLba(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    const LogicalBlockAddress idxOfAllocBitmapBlockModified = (lba >> 3) / kSFSBlockSize;
    const uint8_t* pBlock = &self->allocationBitmap[idxOfAllocBitmapBlockModified * kSFSBlockSize];
    const LogicalBlockAddress allocationBitmapBlockLba = self->allocationBitmapLba + idxOfAllocBitmapBlockModified;

    memset(self->tmpBlock, 0, kSFSBlockSize);
    memcpy(self->tmpBlock, pBlock, &self->allocationBitmap[self->allocationBitmapByteSize] - pBlock);
    return DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, allocationBitmapBlockLba);
}

static errno_t SerenaFS_AllocateBlock(SerenaFSRef _Nonnull self, LogicalBlockAddress* _Nonnull pOutLba)
{
    decl_try_err();
    LogicalBlockAddress lba = 0;    // Safe because LBA #0 is the volume header which is always allocated when the FS is mounted

    Lock_Lock(&self->allocationLock);

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
    Lock_Unlock(&self->allocationLock);

    *pOutLba = lba;
    return EOK;

catch:
    if (lba > 0) {
        AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);
    }
    Lock_Unlock(&self->allocationLock);
    *pOutLba = 0;
    return err;
}

static void SerenaFS_DeallocateBlock(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    if (lba == 0) {
        return;
    }

    Lock_Lock(&self->allocationLock);
    AllocationBitmap_SetBlockInUse(self->allocationBitmap, lba, false);

    // XXX check for error here?
    SerenaFS_WriteBackAllocationBitmapForLba(self, lba);
    Lock_Unlock(&self->allocationLock);
}

errno_t SerenaFS_createNode(SerenaFSRef _Nonnull self, FileType type, User user, FilePermissions permissions, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, SFSDirectoryEntryPointer* _Nullable pDirInsertionHint, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    InodeId parentInodeId = Inode_GetId(pDir);
    LogicalBlockAddress inodeLba = 0;
    LogicalBlockAddress dirContLba = 0;
    FileOffset fileSize = 0ll;
    SFSBlockMap* pBlockMap = NULL;
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

    try(kalloc_cleared(sizeof(SFSBlockMap), (void**)&pBlockMap));
    try(SerenaFS_AllocateBlock(self, &inodeLba));
    
    if (type == kFileType_Directory) {
        // Write the initial directory content. These are just the '.' and '..'
        // entries
        try(SerenaFS_AllocateBlock(self, &dirContLba));

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
        pBlockMap,
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
    kfree(pBlockMap);

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
    const LogicalBlockAddress lba = (LogicalBlockAddress)id;
    SFSBlockMap* pBlockMap = NULL;

    try(kalloc(sizeof(SFSBlockMap), (void**)&pBlockMap));
    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
    const SFSInode* ip = (const SFSInode*)self->tmpBlock;

    for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
        pBlockMap->p[i] = UInt32_BigToHost(ip->blockMap.p[i]);
    }

    return Inode_Create(
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
        pBlockMap,
        pOutNode);

catch:
    kfree(pBlockMap);
    *pOutNode = NULL;
    return err;
}

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

static void SerenaFS_DeallocateFileContentBlocks(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const SFSBlockMap* pBlockMap = (const SFSBlockMap*)Inode_GetBlockMap(pNode);

    for (int i = 0; i < kSFSMaxDirectDataBlockPointers; i++) {
        if (pBlockMap->p[i] == 0) {
            break;
        }

        SerenaFS_DeallocateBlock(self, pBlockMap->p[i]);
    }
}

void SerenaFS_onRemoveNodeFromDisk(SerenaFSRef _Nonnull self, InodeRef _Nonnull pNode)
{
    const LogicalBlockAddress lba = (LogicalBlockAddress)Inode_GetId(pNode);

    SerenaFS_DeallocateFileContentBlocks(self, pNode);
    SerenaFS_DeallocateBlock(self, lba);
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
        pOutEmptyPtr->blockOffset = 0;
        pOutEmptyPtr->fileOffset = 0ll;
    }
    if (pOutEntryPtr) {
        pOutEntryPtr->lba = 0;
        pOutEntryPtr->blockOffset = 0;
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
            pOutEmptyPtr->blockOffset = ((uint8_t*)pEmptyEntry) - ((uint8_t*)pDirBuffer);
            pOutEmptyPtr->fileOffset = offset + pOutEmptyPtr->blockOffset;
        }
        if (hasMatch) {
            break;
        }

        offset += (FileOffset)nBytesAvailable;
    }

    if (hasMatch) {
        if (pOutEntryPtr) {
            pOutEntryPtr->lba = lba;
            pOutEntryPtr->blockOffset = ((uint8_t*)pMatchingEntry) - ((uint8_t*)pDirBuffer);
            pOutEntryPtr->fileOffset = offset + pOutEntryPtr->blockOffset;
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
        try(SerenaFS_AllocateBlock(self, &lba));
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

    if (nBytesToRead > 0) {
        if (offset < 0ll || offset >= kSFSLimit_FileSizeMax) {
            *pOutBytesRead = 0;
            return EOVERFLOW;
        }

        const FileOffset targetOffset = offset + (FileOffset)nBytesToRead;
        if (targetOffset < 0ll || targetOffset > kSFSLimit_FileSizeMax) {
            nBytesToRead = (ssize_t)(kSFSLimit_FileSizeMax - offset);
        }
    }
    else if (nBytesToRead < 0) {
        return EINVAL;
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

    if (nBytesToWrite > 0) {
        if (offset < 0ll || offset >= kSFSLimit_FileSizeMax) {
            *pOutBytesWritten = 0;
            return EOVERFLOW;
        }

        const FileOffset targetOffset = offset + (FileOffset)nBytesToWrite;
        if (targetOffset < 0ll || targetOffset > kSFSLimit_FileSizeMax) {
            nBytesToWrite = (ssize_t)(kSFSLimit_FileSizeMax - offset);
        }
    }
    else if (nBytesToWrite < 0) {
        return EINVAL;
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


errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    err = SELock_LockShared(&self->seLock);
    if (err == EOK) {
        if (self->flags.isMounted) {
            err = Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirLba, pOutDir);
        }
        else {
            err = EIO;
        }
        SELock_Unlock(&self->seLock);
    }
    return err;
}

errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    SFSDirectoryQuery q;
    InodeId entryId;

    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Searchable));
    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, (pDirInsHint) ? (SFSDirectoryEntryPointer*)pDirInsHint->data : NULL, NULL, &entryId, NULL));
    if (pOutNode) {
        try(Filesystem_AcquireNodeWithId((FilesystemRef)self, entryId, pOutNode));
    }
    return EOK;

catch:
    if (pOutNode) {
        *pOutNode = NULL;
    }
    return err;
}

errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, User user, MutablePathComponent* _Nonnull pName)
{
    decl_try_err();
    SFSDirectoryQuery q;

    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Readable | kAccess_Searchable));
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = id;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, NULL, NULL, NULL, pName));
    return EOK;

catch:
    pName->count = 0;
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
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + mp.blockOffset);
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
static errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, const SFSDirectoryEntryPointer* _Nullable pEmptyPtr)
{
    decl_try_err();

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyPtr && pEmptyPtr->lba > 0) {
        // Reuse an empty entry
        try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, pEmptyPtr->lba));
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + pEmptyPtr->blockOffset);

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

            try(SerenaFS_AllocateBlock(self, &lba));
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

errno_t SerenaFS_openDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, User user)
{
    return Filesystem_CheckAccess(self, pDir, user, kAccess_Readable);
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    FileOffset offset = *pInOutOffset;
    SFSDirectoryEntry dirent;
    ssize_t nAllDirBytesRead = 0;
    ssize_t nBytesRead = 0;

    while (nBytesToRead > 0) {
        ssize_t nDirBytesRead;
        const errno_t e1 = SerenaFS_xRead(self, pDir, offset, &dirent, sizeof(SFSDirectoryEntry), &nDirBytesRead);

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

errno_t SerenaFS_openFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, unsigned int mode, User user)
{
    decl_try_err();
    AccessMode accessMode = 0;

    if (Inode_IsDirectory(pFile)) {
        throw(EISDIR);
    }

    if ((mode & kOpen_ReadWrite) == 0) {
        throw(EACCESS);
    }
    if ((mode & kOpen_Read) == kOpen_Read) {
        accessMode |= kAccess_Readable;
    }
    if ((mode & kOpen_Write) == kOpen_Write || (mode & kOpen_Truncate) == kOpen_Truncate) {
        accessMode |= kAccess_Writable;
    }

    try(Filesystem_CheckAccess(self, pFile, user, accessMode));


    // A negative file size is treated as an overflow
    if (Inode_GetFileSize(pFile) < 0ll || Inode_GetFileSize(pFile) > kSFSLimit_FileSizeMax) {
        throw(EOVERFLOW);
    }


    if ((mode & kOpen_Truncate) == kOpen_Truncate) {
        SerenaFS_xTruncateFile(self, pFile, 0);
    }
    
catch:
    return err;
}

errno_t SerenaFS_readFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();

    err = SerenaFS_xRead(self, pFile, *pInOutOffset, pBuffer, nBytesToRead, nOutBytesRead);
    *pInOutOffset += *nOutBytesRead;
    return err;
}

errno_t SerenaFS_writeFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();

    err = SerenaFS_xWrite(self, pFile, *pInOutOffset, pBuffer, nBytesToWrite, nOutBytesWritten);
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
            SerenaFS_DeallocateBlock(self, pBlockMap->p[i]);
            pBlockMap->p[i] = 0;
        }
    }

    Inode_SetFileSize(pNode, length);
    Inode_SetModified(pNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}

errno_t SerenaFS_truncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pFile, User user, FileOffset length)
{
    decl_try_err();

    try(Filesystem_CheckAccess(self, pFile, user, kAccess_Writable));

    const FileOffset oldLength = Inode_GetFileSize(pFile);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(pFile, length);
        Inode_SetModified(pFile, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SerenaFS_xTruncateFile(self, pFile, length);
    }

catch:
    return err;
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
