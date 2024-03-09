//
//  SerenaFS.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <driver/MonotonicClock.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode extensions
////////////////////////////////////////////////////////////////////////////////

// Returns true if the given directory node is empty (contains just "." and "..").
static bool DirectoryNode_IsEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetFileSize(self) <= sizeof(SFSDirectoryEntry) * 2;
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
    Bytes_ClearRange(p, diskBlockSize);
    SFSVolumeHeader* vhp = (SFSVolumeHeader*)p;
    vhp->signature = kSFSSignature_SerenaFS;
    vhp->version = kSFSVersion_Current;
    vhp->attributes = 0;
    vhp->creationTime = curTime;
    vhp->modificationTime = curTime;
    vhp->blockSize = diskBlockSize;
    vhp->volumeBlockCount = diskBlockCount;
    vhp->allocationBitmapByteSize = allocationBitmapByteSize;
    vhp->rootDirectory = rootDirInodeLba;
    vhp->allocationBitmap = 1;
    try(DiskDriver_PutBlock(pDriver, vhp, 0));


    // Write the allocation bitmap
    // Note that we mark the blocks that we already know are in use as in-use
    const size_t nAllocationBitsPerBlock = diskBlockSize << 3;
    const LogicalBlockAddress nBlocksToAllocate = 1 + allocBitmapBlockCount + 1 + 1; // volume header + alloc bitmap + root dir inode + root dir content
    LogicalBlockAddress nBlocksAllocated = 0;

    for (LogicalBlockAddress i = 0; i < allocBitmapBlockCount; i++) {
        Bytes_ClearRange(p, diskBlockSize);

        LogicalBlockAddress bitNo = 0;
        while (nBlocksAllocated < __min(nBlocksToAllocate, nAllocationBitsPerBlock)) {
            AllocationBitmap_SetBlockInUse(p, bitNo, true);
            nBlocksAllocated++;
            bitNo++;
        }

        try(DiskDriver_PutBlock(pDriver, p, 1 + i));
    }


    // Write the root directory inode
    Bytes_ClearRange(p, diskBlockSize);
    SFSInode* ip = (SFSInode*)p;
    ip->accessTime = curTime;
    ip->modificationTime = curTime;
    ip->statusChangeTime = curTime;
    ip->size = 2 * sizeof(SFSDirectoryEntry);
    ip->uid = user.uid;
    ip->gid = user.gid;
    ip->permissions = permissions;
    ip->linkCount = 1;
    ip->type = kFileType_Directory;
    ip->blockMap.p[0] = rootDirContentLba;
    try(DiskDriver_PutBlock(pDriver, ip, rootDirInodeLba));


    // Write the root directory content. This is just the entries '.' and '..'
    // which both point back to the root directory.
    Bytes_ClearRange(p, diskBlockSize);
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)p;
    dep[0].id = rootDirInodeLba;
    dep[0].filename[0] = '.';
    dep[1].id = rootDirInodeLba;
    dep[1].filename[0] = '.';
    dep[1].filename[1] = '.';
    try(DiskDriver_PutBlock(pDriver, dep, rootDirContentLba));

catch:
    kfree(p);
    return err;
}

// Creates an instance of SerenaFS. SerenaFS is a volatile file system that does not
// survive system restarts. The 'rootDirUser' parameter specifies the user and
// group ID of the root directory.
errno_t SerenaFS_Create(User rootDirUser, SerenaFSRef _Nullable * _Nonnull pOutFileSys)
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

    *pOutFileSys = self;
    return EOK;

catch:
    *pOutFileSys = NULL;
    return err;
}

void SerenaFS_deinit(SerenaFSRef _Nonnull self)
{
    // Can not be that we are getting deallocated while being mounted
    assert(self->diskDriver == NULL);
    ConditionVariable_Deinit(&self->notifier);
    Lock_Deinit(&self->lock);
}

static errno_t SerenaFS_WriteBackAllocationBitmapForLba(SerenaFSRef _Nonnull self, LogicalBlockAddress lba)
{
    const LogicalBlockAddress idxOfAllocBitmapBlockModified = (lba >> 3) / kSFSBlockSize;
    const uint8_t* pBlock = &self->allocationBitmap[idxOfAllocBitmapBlockModified * kSFSBlockSize];
    const LogicalBlockAddress allocationBitmapBlockLba = self->allocationBitmapLba + idxOfAllocBitmapBlockModified;

    return DiskDriver_PutBlock(self->diskDriver, pBlock, allocationBitmapBlockLba);
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

// Invoked when Filesystem_AllocateNode() is called. Subclassers should
// override this method to allocate and initialize an inode of the given type.
errno_t SerenaFS_onAllocateNodeOnDisk(SerenaFSRef _Nonnull self, FileType type, void* _Nullable pContext, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    LogicalBlockAddress lba = 0;
    void* pBlockMap = NULL;

    try(kalloc_cleared(sizeof(SFSBlockMap), &pBlockMap));
    try(SerenaFS_AllocateBlock_Locked(self, &lba));

    try(Inode_Create(
        Filesystem_GetId(self),
        (InodeId)lba,
        type,
        1,
        0,      // XXX clarify whether we want to assign some user, group and permissions here
        0,
        0,
        0,
        curTime,
        curTime,
        curTime,
        pBlockMap,
        pOutNode));
    return EOK;

catch:
    kfree(pBlockMap);
    SerenaFS_DeallocateBlock_Locked(self, lba);
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
    void* pBlockMap = NULL;

    try(kalloc(sizeof(SFSBlockMap), &pBlockMap));
    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));

    const SFSInode* ip = (const SFSInode*)self->tmpBlock;
    Bytes_CopyRange(pBlockMap, &ip->blockMap, sizeof(SFSBlockMap));
    return Inode_Create(
        Filesystem_GetId(self),
        id,
        ip->type,
        ip->linkCount,
        ip->uid,
        ip->gid,
        ip->permissions,
        ip->size,
        ip->accessTime,
        ip->modificationTime,
        ip->statusChangeTime,
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

    Bytes_ClearRange(ip, kSFSBlockSize);

    ip->accessTime = (Inode_IsAccessed(pNode)) ? curTime : Inode_GetAccessTime(pNode);
    ip->modificationTime = (Inode_IsUpdated(pNode)) ? curTime : Inode_GetModificationTime(pNode);
    ip->statusChangeTime = (Inode_IsStatusChanged(pNode)) ? curTime : Inode_GetStatusChangeTime(pNode);
    ip->size = Inode_GetFileSize(pNode);
    ip->uid = Inode_GetUserId(pNode);
    ip->gid = Inode_GetGroupId(pNode);
    ip->permissions = Inode_GetFilePermissions(pNode);
    ip->linkCount = Inode_GetLinkCount(pNode);
    ip->type = Inode_GetFileType(pNode);
    Bytes_CopyRange(&ip->blockMap, pBlockMap, sizeof(SFSBlockMap));

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

// Points to a directory entry inside a disk block
typedef struct SFSDirectoryEntryPointer {
    LogicalBlockAddress     lba;        // LBA of the disk block that holds the directory entry
    size_t                  offset;     // Byte offset to the directory entry relative to the dis block start
    FileOffset              fileOffset; // Byte offset relative to the start of the directory file
} SFSDirectoryEntryPointer;

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

    while (true) {
        const int blockIdx = offset >> (FileOffset)kSFSBlockSizeShift;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)kSFSBlockSize, fileSize - offset);

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Read, &lba));
        if (lba == 0) {
            Bytes_ClearRange(self->tmpBlock, kSFSBlockSize);
        }
        else {
            try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
        }

        const int nDirEntries = nBytesAvailable / sizeof(SFSDirectoryEntry);
        hasMatch = xHasMatchingDirectoryEntry(pQuery, pDirBuffer, nDirEntries, &pEmptyEntry, &pMatchingEntry);
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
            *pOutId = pMatchingEntry->id;
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
// This functions reads a block full of data from teh backing store and then
// invokes 'cb' with this block of data. 'cb' is expected to process the data.
// Note that 'cb' may process just a subset of the data and it returns how much
// of the data it has processed. This amount of bytes is then subtracted from
// 'nBytesToRead'. However the offset is always advanced by a full block size.
// This process continues until 'nBytesToRead' has decreased to 0, EOF or an
// error is encountered. Whatever comes first. 
static errno_t SerenaFS_xRead(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ssize_t nBytesToRead, SFSReadCallback _Nonnull cb, void* _Nullable pContext, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    ssize_t nOriginalBytesToRead = nBytesToRead;

    if (offset < 0ll) {
        *pOutBytesRead = 0;
        return EINVAL;
    }

    while (nBytesToRead > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kSFSBlockSizeShift);   //XXX blockIdx should be 64bit
        const ssize_t blockOffset = offset & (FileOffset)kSFSBlockSizeMask;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)(kSFSBlockSize - blockOffset), __min(fileSize - offset, (FileOffset)nBytesToRead));
        LogicalBlockAddress lba;

        if (nBytesAvailable <= 0) {
            break;
        }

        errno_t e1 = SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Read, &lba);
        if (e1 == EOK) {
            if (lba == 0) {
                Bytes_ClearRange(self->tmpBlock, kSFSBlockSize);
            }
            else {
                e1 = DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba);
            }
        }
        if (e1 != EOK) {
            err = (nBytesToRead == nOriginalBytesToRead) ? e1 : EOK;
            break;
        }

        nBytesToRead -= cb(pContext, self->tmpBlock + blockOffset, nBytesAvailable);
        offset += (FileOffset)nBytesAvailable;
    }

    *pOutBytesRead = nOriginalBytesToRead - nBytesToRead;
    if (*pOutBytesRead > 0) {
        Inode_SetModified(pNode, kInodeFlag_Accessed);
    }
    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'pNode' starting at offset 'offset'.
// 'cb' is used to copy the data from teh source to the disk block(s).
static errno_t SerenaFS_xWrite(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, ssize_t nBytesToWrite, SFSWriteCallback _Nonnull cb, void* _Nullable pContext, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    ssize_t nBytesWritten = 0;

    if (offset < 0ll) {
        *pOutBytesWritten = 0;
        return EINVAL;
    }

    while (nBytesToWrite > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kSFSBlockSizeShift);   //XXX blockIdx should be 64bit
        const ssize_t blockOffset = offset & (FileOffset)kSFSBlockSizeMask;
        const ssize_t nBytesAvailable = __min(kSFSBlockSize - blockOffset, nBytesToWrite);
        LogicalBlockAddress lba;

        errno_t e1 = SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Write, &lba);
        if (e1 == EOK) {
            if (lba == 0) {
                Bytes_ClearRange(self->tmpBlock, kSFSBlockSize);
            }
            else {
                e1 = DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba);
            }
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }
        
        cb(self->tmpBlock + blockOffset, pContext, nBytesAvailable);
        e1 = DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, lba);
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesWritten += nBytesAvailable;
        offset += (FileOffset)nBytesAvailable;
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
    if (vhp->signature != kSFSSignature_SerenaFS || vhp->version != kSFSVersion_v1) {
        throw(EIO);
    }
    if (vhp->blockSize != kSFSBlockSize || vhp->volumeBlockCount < kSFSVolume_MinBlockCount || vhp->allocationBitmapByteSize < 1) {
        throw(EIO);
    }

    const size_t diskBlockSize = vhp->blockSize;
    size_t allocBitmapByteSize = vhp->allocationBitmapByteSize;


    // Cache the root directory info
    self->rootDirLba = vhp->rootDirectory;


    // Cache the allocation bitmap in RAM
    self->allocationBitmapLba = vhp->allocationBitmap;
    self->allocationBitmapBlockCount = (allocBitmapByteSize + (diskBlockSize - 1)) / diskBlockSize;
    self->allocationBitmapByteSize = allocBitmapByteSize;
    self->volumeBlockCount = vhp->volumeBlockCount;

    try(kalloc(allocBitmapByteSize, (void**)&self->allocationBitmap));
    uint8_t* pAllocBitmap = self->allocationBitmap;

    for (LogicalBlockAddress lba = 0; lba < self->allocationBitmapBlockCount; lba++) {
        const size_t nBytesToCopy = __min(kSFSBlockSize, allocBitmapByteSize);

        try(DiskDriver_GetBlock(pDriver, self->tmpBlock, self->allocationBitmapLba + lba));
        Bytes_CopyRange(pAllocBitmap, self->tmpBlock, nBytesToCopy);
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
    Bytes_ClearRange(dep, sizeof(SFSDirectoryEntry));
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
        dep->id = id;

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
            Bytes_ClearRange(self->tmpBlock, kSFSBlockSize);
            dep = (SFSDirectoryEntry*)self->tmpBlock;
        }

        String_CopyUpTo(dep->filename, pName->name, pName->count);
        dep->id = id;
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

static errno_t SerenaFS_CreateDirectoryDiskNode(SerenaFSRef _Nonnull self, InodeId parentId, UserId uid, GroupId gid, FilePermissions permissions, InodeId* _Nonnull pOutId)
{
    decl_try_err();
    InodeRef _Locked pDirNode = NULL;

    try(Filesystem_AllocateNode((FilesystemRef)self, kFileType_Directory, uid, gid, permissions, NULL, &pDirNode));
    const InodeId id = Inode_GetId(pDirNode);

    try(SerenaFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Self, id, NULL));
    try(SerenaFS_InsertDirectoryEntry(self, pDirNode, &kPathComponent_Parent, (parentId > 0) ? parentId : id, NULL));

    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = id;
    return EOK;

catch:
    Filesystem_RelinquishNode((FilesystemRef)self, pDirNode);
    *pOutId = 0;
    return err;
}

// Creates an empty directory as a child of the given directory node and with
// the given name, user and file permissions. Returns EEXIST if a node with
// the given name already exists.
errno_t SerenaFS_createDirectory(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, FilePermissions permissions)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


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


    // Create the new directory and add it to its parent directory
    InodeId newDirId = 0;
    try(SerenaFS_CreateDirectoryDiskNode(self, Inode_GetId(pParentNode), user.uid, user.gid, permissions, &newDirId));
    try(SerenaFS_InsertDirectoryEntry(self, pParentNode, pName, newDirId, &ep));
    return EOK;

catch:
    // XXX Unlink new dir disk node
    return err;
}

// Opens the directory represented by the given node. Returns a directory
// descriptor object which is the I/O channel that allows you to read the
// directory content.
errno_t SerenaFS_openDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, User user, DirectoryRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    try(Inode_CheckAccess(pDirNode, user, kFilePermission_Read));
    try(Directory_Create((FilesystemRef)self, pDirNode, pOutDir));

catch:
    return err;
}

// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'pDir'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
static ssize_t xCopyOutDirectoryEntries(DirectoryEntry* _Nonnull pOut, const SFSDirectoryEntry* _Nonnull pIn, ssize_t nBytesToRead)
{
    ssize_t nBytesCopied = 0;

    while (nBytesToRead > 0) {
        if (pIn->id > 0) {
            pOut->inodeId = pIn->id;
            String_CopyUpTo(pOut->name, pIn->filename, kSFSMaxFilenameLength);
            nBytesCopied += sizeof(SFSDirectoryEntry);
            pOut++;
        }
        pIn++;
        nBytesToRead -= sizeof(SFSDirectoryEntry);
    }

    return nBytesCopied;
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, DirectoryRef _Nonnull pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    InodeRef _Locked pNode = Directory_GetInode(pDir);
    const ssize_t nBytesToReadFromDirectory = (nBytesToRead / sizeof(DirectoryEntry)) * sizeof(SFSDirectoryEntry);
    ssize_t nBytesRead;

    // XXX reading multiple entries at once doesn't work right because xRead advances 'pBuffer' by sizeof(RamDirectoryEntry) rather
    // XXX than DirectoryEntry. Former is 32 bytes and later is 260 bytes.
    // XXX the Directory_GetOffset() should really return the number of the entry rather than a byte offset
    const errno_t err = SerenaFS_xRead(self, 
        pNode, 
        Directory_GetOffset(pDir),
        nBytesToReadFromDirectory,
        (SFSReadCallback)xCopyOutDirectoryEntries,
        pBuffer,
        &nBytesRead);
    Directory_IncrementOffset(pDir, nBytesRead);
    *nOutBytesRead = (nBytesRead / sizeof(SFSDirectoryEntry)) * sizeof(DirectoryEntry);
    return err;
}

// Creates an empty file and returns the inode of that file. The behavior is
// non-exclusive by default. Meaning the file is created if it does not 
// exist and the file's inode is merrily acquired if it already exists. If
// the mode is exclusive then the file is created if it doesn't exist and
// an error is thrown if the file exists. Note that the file is not opened.
// This must be done by calling the open() method.
errno_t SerenaFS_createFile(SerenaFSRef _Nonnull self, const PathComponent* _Nonnull pName, InodeRef _Nonnull _Locked pParentNode, User user, unsigned int options, FilePermissions permissions, InodeRef _Nullable _Locked * _Nonnull pOutNode)
{
    decl_try_err();

    // 'pParentNode' must be a directory
    if (!Inode_IsDirectory(pParentNode)) {
        throw(ENOTDIR);
    }


    // We must have write permissions for 'pParentNode'
    try(SerenaFS_CheckAccess_Locked(self, pParentNode, user, kFilePermission_Write));


    // Make sure that 'pParentNode' doesn't already have an entry with name 'pName'.
    // Also figure out whether there's an empty entry that we can reuse.
    InodeId existingFileId;
    SFSDirectoryEntryPointer ep;
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    err = SerenaFS_GetDirectoryEntry(self, pParentNode, &q, &ep, NULL, &existingFileId, NULL);
    if (err == ENOENT) {
        err = EOK;
    } else if (err == EOK) {
        if ((options & kOpen_Exclusive) == kOpen_Exclusive) {
            // Exclusive mode: File already exists -> throw an error
            throw(EEXIST);
        }
        else {
            // Non-exclusive mode: File already exists -> acquire it and let the caller open it
            try(Filesystem_AcquireNodeWithId((FilesystemRef)self, existingFileId, NULL, pOutNode));

            // Truncate the file to length 0, if requested
            if ((options & kOpen_Truncate) == kOpen_Truncate) {
                SerenaFS_xTruncateFile(self, *pOutNode, 0);
            }

            return EOK;
        }
    } else {
        throw(err);
    }


    // Create the new file and add it to its parent directory
    try(Filesystem_AllocateNode((FilesystemRef)self, kFileType_RegularFile, user.uid, user.gid, permissions, NULL, pOutNode));
    try(SerenaFS_InsertDirectoryEntry(self, pParentNode, pName, Inode_GetId(*pOutNode), &ep));

    return EOK;

catch:
    // XXX Unlink new file disk node if necessary
    return err;
}

// Opens a resource context/channel to the resource. This new resource context
// will be represented by a (file) descriptor in user space. The resource context
// maintains state that is specific to this connection. This state will be
// protected by the resource's internal locking mechanism. 'pNode' represents
// the named resource instance that should be represented by the I/O channel.
errno_t SerenaFS_open(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, unsigned int mode, User user, FileRef _Nullable * _Nonnull pOutFile)
{
    decl_try_err();
    FilePermissions permissions = 0;

    if (Inode_IsDirectory(pNode)) {
        throw(EISDIR);
    }

    if ((mode & kOpen_ReadWrite) == 0) {
        throw(EACCESS);
    }
    if ((mode & kOpen_Read) != 0) {
        permissions |= kFilePermission_Read;
    }
    if ((mode & kOpen_Write) != 0) {
        permissions |= kFilePermission_Write;
    }

    try(Inode_CheckAccess(pNode, user, permissions));
    try(File_Create((FilesystemRef)self, mode, pNode, pOutFile));

    if ((mode & kOpen_Truncate) != 0) {
        SerenaFS_xTruncateFile(self, pNode, 0);
    }
    
catch:
    return err;
}

// Close the resource. The purpose of the close operation is:
// - flush all data that was written and is still buffered/cached to the underlying device
// - if a write operation is ongoing at the time of the close then let this write operation finish and sync the underlying device
// - if a read operation is ongoing at the time of the close then interrupt the read with an EINTR error
// The resource should be internally marked as closed and all future read/write/etc operations on the resource should do nothing
// and instead return a suitable status. Eg a write should return EIO and a read should return EOF.
// It is permissible for a close operation to block the caller for some (reasonable) amount of time to complete the flush.
// The close operation may return an error. Returning an error will not stop the kernel from completing the close and eventually
// deallocating the resource. The error is passed on to the caller but is purely advisory in nature. The close operation is
// required to mark the resource as closed whether the close internally succeeded or failed. 
errno_t SerenaFS_close(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile)
{
    // Nothing to do for now
    return EOK;
}

static ssize_t xCopyOutFileContent(void* _Nonnull pOut, const void* _Nonnull pIn, ssize_t nBytesToRead)
{
    Bytes_CopyRange(pOut, pIn, nBytesToRead);
    return nBytesToRead;
}

errno_t SerenaFS_read(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    InodeRef _Locked pNode = File_GetInode(pFile);

    const errno_t err = SerenaFS_xRead(self, 
        pNode, 
        File_GetOffset(pFile),
        nBytesToRead,
        (SFSReadCallback)xCopyOutFileContent,
        pBuffer,
        nOutBytesRead);
    File_IncrementOffset(pFile, *nOutBytesRead);
    return err;
}

errno_t SerenaFS_write(SerenaFSRef _Nonnull self, FileRef _Nonnull pFile, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    InodeRef _Locked pNode = File_GetInode(pFile);
    FileOffset offset;

    if (File_IsAppendOnWrite(pFile)) {
        offset = Inode_GetFileSize(pNode);
    } else {
        offset = File_GetOffset(pFile);
    }

    const errno_t err = SerenaFS_xWrite(self, 
        pNode, 
        offset,
        nBytesToWrite,
        (SFSWriteCallback)Bytes_CopyRange,
        pBuffer,
        nOutBytesWritten);
    File_IncrementOffset(pFile, *nOutBytesWritten);
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
    if (Inode_IsDirectory(pNodeToUnlink) && !DirectoryNode_IsEmpty(pNodeToUnlink)) {
        throw(EBUSY);
    }


    // Remove the directory entry in the parent directory
    try(SerenaFS_RemoveDirectoryEntry(self, pParentNode, Inode_GetId(pNodeToUnlink)));
    SerenaFS_xTruncateFile(self, pParentNode, Inode_GetFileSize(pParentNode));


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
OVERRIDE_METHOD_IMPL(onAllocateNodeOnDisk, SerenaFS, Filesystem)
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
OVERRIDE_METHOD_IMPL(createDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(openDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(readDirectory, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(open, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(close, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(read, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(write, SerenaFS, IOResource)
OVERRIDE_METHOD_IMPL(truncate, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(checkAccess, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(unlink, SerenaFS, Filesystem)
OVERRIDE_METHOD_IMPL(rename, SerenaFS, Filesystem)
);
