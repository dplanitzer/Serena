//
//  SfsFile.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsFile.h"
#include "SerenaFSPriv.h"
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>
#include <System/ByteOrder.h>


errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, InodeId inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    SfsFileRef self;

    err = Inode_Create(
        pClass,
        (FilesystemRef)fs,
        inid,
        ip->type,
        Int32_BigToHost(ip->linkCount),
        UInt32_BigToHost(ip->uid),
        UInt32_BigToHost(ip->gid),
        UInt16_BigToHost(ip->permissions),
        Int64_BigToHost(ip->size),
        TimeInterval_Make(UInt32_BigToHost(ip->accessTime.tv_sec), UInt32_BigToHost(ip->accessTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->modificationTime.tv_sec), UInt32_BigToHost(ip->modificationTime.tv_nsec)),
        TimeInterval_Make(UInt32_BigToHost(ip->statusChangeTime.tv_sec), UInt32_BigToHost(ip->statusChangeTime.tv_nsec)),
        (InodeRef*)&self);
    if (err == EOK) {
        memcpy(self->direct, ip->bp, sizeof(sfs_bno_t) * kSFSInodeBlockPointersCount);
    }
    *pOutNode = (InodeRef)self;

    return err;
}

void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip)
{
    SfsFileRef self = (SfsFileRef)pNode;
    const TimeInterval curTime = FSGetCurrentTime();
    const TimeInterval accTime = (Inode_IsAccessed(self)) ? curTime : Inode_GetAccessTime(self);
    const TimeInterval modTime = (Inode_IsUpdated(self)) ? curTime : Inode_GetModificationTime(self);
    const TimeInterval chgTime = (Inode_IsStatusChanged(self)) ? curTime : Inode_GetStatusChangeTime(self);

    ip->accessTime.tv_sec = UInt32_HostToBig(accTime.tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(accTime.tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(modTime.tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(modTime.tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(chgTime.tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(chgTime.tv_nsec);
    ip->size = Int64_HostToBig(Inode_GetFileSize(self));
    ip->uid = UInt32_HostToBig(Inode_GetUserId(self));
    ip->gid = UInt32_HostToBig(Inode_GetGroupId(self));
    ip->linkCount = Int32_HostToBig(Inode_GetLinkCount(self));
    ip->permissions = UInt16_HostToBig(Inode_GetFilePermissions(self));
    ip->type = Inode_GetFileType(self);

    memcpy(ip->bp, SfsFile_GetBlockMap(self), sizeof(uint32_t) * kSFSInodeBlockPointersCount);
}

// Acquires the disk block 'lba' if lba is > 0; otherwise allocates a new block.
// The new block is for read-only if read-only 'mode' is requested and it is
// suitable for writing back to disk if 'mode' is a replace/update mode.
static errno_t acquire_disk_block(SerenaFSRef _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, sfs_bno_t* _Nonnull pOutLba, bool* _Nonnull pOutIsAlloc, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);

    *pOutIsAlloc = false;

    if (lba > 0) {
        err = FSContainer_AcquireBlock(fsContainer, lba, mode, pOutBlock);
    }
    else {
        if (mode == kAcquireBlock_ReadOnly) {
            err = FSContainer_AcquireEmptyBlock(fsContainer, pOutBlock);
        }
        else {
            LogicalBlockAddress new_lba;

            if((err = SfsAllocator_Allocate(&self->blockAllocator, &new_lba)) == EOK) {
                *pOutLba = UInt32_HostToBig(new_lba);
                *pOutIsAlloc = true;

                err = FSContainer_AcquireBlock(fsContainer, new_lba, kAcquireBlock_Cleared, pOutBlock);
            }
        }
    }

    return err;
}

// Acquires the file block 'fba' in the file 'self'. Note that this function
// allocates a new file block if 'mode' implies a write operation and the required
// file block doesn't exist yet. However this function does not commit the updated
// allocation bitmap back to disk. The caller has to trigger this.
// This function expects that 'fba' is in the range 0..<numBlocksInFile.
errno_t SfsFile_AcquireBlock(SfsFileRef _Nonnull _Locked self, sfs_bno_t fba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    sfs_bno_t* ino_bmap = SfsFile_GetBlockMap(self);
    bool isAlloc;

    if (fba < kSFSDirectBlockPointersCount) {
        LogicalBlockAddress dat_lba = UInt32_BigToHost(ino_bmap[fba]);

        return acquire_disk_block(fs, dat_lba, mode, &ino_bmap[fba], &isAlloc, pOutBlock);
    }
    fba -= kSFSDirectBlockPointersCount;


    if (fba < kSFSBlockPointersPerBlockCount) {
        DiskBlockRef i0_block;
        LogicalBlockAddress i0_lba = UInt32_BigToHost(ino_bmap[kSFSDirectBlockPointersCount]);

        // Get the indirect block
        try(acquire_disk_block(fs, i0_lba, kAcquireBlock_Update, &ino_bmap[kSFSDirectBlockPointersCount], &isAlloc, &i0_block));


        // Get the data block
        sfs_bno_t* i0_bmap = DiskBlock_GetMutableData(i0_block);
        LogicalBlockAddress dat_lba = UInt32_BigToHost(i0_bmap[fba]);

        err = acquire_disk_block(fs, dat_lba, mode, &i0_bmap[fba], &isAlloc, pOutBlock);
        
        if (isAlloc) {
            FSContainer_RelinquishBlockWriting(fsContainer, i0_block, kWriteBlock_Sync);
        }
        else {
            FSContainer_RelinquishBlock(fsContainer, i0_block);
        }

        return err;
    }

    err = EFBIG;

catch:
    *pOutBlock = NULL;
    return err;
}

void SfsFile_ConvertOffset(SfsFileRef _Nonnull _Locked self, FileOffset offset, sfs_bno_t* _Nonnull pOutBlockIdx, ssize_t* _Nonnull pOutBlockOffset)
{
    *pOutBlockIdx = (sfs_bno_t)(offset >> (FileOffset)kSFSBlockSizeShift);
    *pOutBlockOffset = (ssize_t)(offset & (FileOffset)kSFSBlockSizeMask);
}

// Reads 'nBytesToRead' bytes from the file 'pNode' starting at offset 'offset'.
errno_t SfsFile_xRead(SfsFileRef _Nonnull _Locked self, FileOffset offset, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    uint8_t* dp = buf;
    ssize_t nBytesRead = 0;

    if (nBytesToRead < 0) {
        throw(EINVAL);
    }
    if (nBytesToRead > 0 && offset < 0ll) {
        throw(EOVERFLOW);
    }


    // Limit 'nBytesToRead' to the range of bytes that is actually available in
    // the file starting at 'offset'.
    const FileOffset fileSize = Inode_GetFileSize(self);
    const FileOffset nAvailBytes = fileSize - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (FileOffset)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToRead) {
            nBytesToRead = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToRead' as is
    } else {
        nBytesToRead = 0;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    sfs_bno_t blockIdx;
    ssize_t blockOffset;
    SfsFile_ConvertOffset(self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    while (nBytesToRead > 0) {
        const ssize_t nActualBlockSize = kSFSBlockSize - blockOffset;
        const ssize_t nBytesToReadInBlock = (nBytesToRead > nActualBlockSize) ? nActualBlockSize : nBytesToRead;
        DiskBlockRef pBlock;

        const errno_t e1 = SfsFile_AcquireBlock(self, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        const uint8_t* bp = DiskBlock_GetData(pBlock);
        memcpy(dp, bp + blockOffset, nBytesToReadInBlock);
        FSContainer_RelinquishBlock(fsContainer, pBlock);

        nBytesToRead -= nBytesToReadInBlock;
        nBytesRead += nBytesToReadInBlock;
        dp += nBytesToReadInBlock;

        blockOffset = 0;
        blockIdx++;
    }


    if (nBytesRead > 0 && fs->mountFlags.isAccessUpdateOnReadEnabled) {
        Inode_SetModified(self, kInodeFlag_Accessed);
    }

catch:
    *pOutBytesRead = nBytesRead;
    return err;
}

// Internal file truncation function. Shortens the file 'self' to the new and
// smaller size 'length'. Does not support increasing the size of a file.
void SfsFile_xTruncate(SfsFileRef _Nonnull _Locked self, FileOffset newLength)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const FileOffset oldLength = Inode_GetFileSize(self);
    sfs_bno_t* ino_bmap = SfsFile_GetBlockMap(self);
    const sfs_bno_t bn_nlen = (sfs_bno_t)(newLength >> (FileOffset)kSFSBlockSizeShift);   //XXX should be 64bit
    const size_t boff_nlen = newLength & (FileOffset)kSFSBlockSizeMask;
    sfs_bno_t bn_first_to_discard = (boff_nlen > 0) ? bn_nlen + 1 : bn_nlen;   // first block to discard (the block that contains newLength or that is right in front of newLength)

    if (bn_first_to_discard < kSFSDirectBlockPointersCount) {
        for (sfs_bno_t bn = bn_first_to_discard; bn < kSFSDirectBlockPointersCount; bn++) {
            if (ino_bmap[bn] != 0) {
                SfsAllocator_Deallocate(&fs->blockAllocator, UInt32_BigToHost(ino_bmap[bn]));
                ino_bmap[bn] = 0;
            }
        }
    }

    const sfs_bno_t bn_first_i1_to_discard = (bn_first_to_discard < kSFSDirectBlockPointersCount) ? 0 : bn_first_to_discard - kSFSDirectBlockPointersCount;
    const LogicalBlockAddress i1_lba = UInt32_BigToHost(ino_bmap[kSFSDirectBlockPointersCount]);

    if (i1_lba != 0) {
        DiskBlockRef pBlock;

        err = FSContainer_AcquireBlock(fsContainer, i1_lba, kAcquireBlock_Update, &pBlock);
        if (err == EOK) {
            sfs_bno_t* i1_bmap = (sfs_bno_t*)DiskBlock_GetMutableData(pBlock);

            for (sfs_bno_t bn = bn_first_i1_to_discard; bn < kSFSBlockPointersPerBlockCount; bn++) {
                if (i1_bmap[bn] != 0) {
                    SfsAllocator_Deallocate(&fs->blockAllocator, UInt32_BigToHost(i1_bmap[bn]));
                    i1_bmap[bn] = 0;
                }
            }

            if (bn_first_i1_to_discard == 0) {
                // We removed the whole i1 level
                ino_bmap[kSFSDirectBlockPointersCount] = 0;
                // XXX no need to write back the abandoned i1 block?
            }
            else {
                // We partially removed the i1 level
            }

            FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        }
    }

    SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer);

    Inode_SetFileSize(self, newLength);
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
}


class_func_defs(SfsFile, Inode,
);
