//
//  SerenaFS_File.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


// Acquires the disk block 'lba' if lba is > 0; otherwise allocates a new block.
// The new block is for read-only if read-only 'mode' is requested and it is
// suitable for writing back to disk if 'mode' is a replace/update mode.
static errno_t acquire_disk_block(SerenaFSRef _Nonnull self, LogicalBlockAddress lba, AcquireBlock mode, SFSBlockNumber* _Nonnull pOutLba, bool* _Nonnull pOutIsAlloc, DiskBlockRef _Nullable * _Nonnull pOutBlock)
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

            if((err = SerenaFS_AllocateBlock(self, &new_lba)) == EOK) {
                *pOutLba = UInt32_HostToBig(new_lba);
                *pOutIsAlloc = true;

                err = FSContainer_AcquireBlock(fsContainer, new_lba, kAcquireBlock_Cleared, pOutBlock);
            }
        }
    }

    return err;
}

// Acquires the file block 'fba' in the file 'pInode'.
// XXX 'fba' should be LogicalBlockAddress. However we want to be able to detect overflows
errno_t SerenaFS_AcquireFileBlock(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, int fba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    SFSBlockNumber* ino_bmap = Inode_GetBlockMap(pNode);
    bool isAlloc;

    if (fba < 0) {
        throw(EFBIG);
    }

    if (fba < kSFSDirectBlockPointersCount) {
        LogicalBlockAddress dat_lba = UInt32_BigToHost(ino_bmap[fba]);

        return acquire_disk_block(self, dat_lba, mode, &ino_bmap[fba], &isAlloc, pOutBlock);
    }
    fba -= kSFSDirectBlockPointersCount;


    if (fba < kSFSBlockPointersPerBlockCount) {
        DiskBlockRef i0_block;
        LogicalBlockAddress i0_lba = UInt32_BigToHost(ino_bmap[kSFSDirectBlockPointersCount]);

        // Get the indirect block
        try(acquire_disk_block(self, i0_lba, kAcquireBlock_Update, &ino_bmap[kSFSDirectBlockPointersCount], &isAlloc, &i0_block));


        // Get the data block
        SFSBlockNumber* i0_bmap = DiskBlock_GetMutableData(i0_block);
        LogicalBlockAddress dat_lba = UInt32_BigToHost(i0_bmap[fba]);

        err = acquire_disk_block(self, dat_lba, mode, &i0_bmap[fba], &isAlloc, pOutBlock);
        
        if (isAlloc) {
            FSContainer_RelinquishBlockWriting(fsContainer, i0_block, kWriteBlock_Sync);
        }
        else {
            FSContainer_RelinquishBlock(fsContainer, i0_block);
        }

        return err;
    }

    throw(EFBIG);

catch:
    *pOutBlock = NULL;
    return err;
}

// Reads 'nBytesToRead' bytes from the file 'pNode' starting at offset 'offset'.
errno_t SerenaFS_xRead(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    uint8_t* dp = pBuffer;
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
        DiskBlockRef pBlock;

        errno_t e1 = SerenaFS_AcquireFileBlock(self, pNode, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
        if (e1 == EOK) {
            const uint8_t* bp = DiskBlock_GetData(pBlock);
            
            memcpy(dp, bp + blockOffset, nBytesToReadInCurrentBlock);
            FSContainer_RelinquishBlock(fsContainer, pBlock);
        }
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }

        nBytesToRead -= nBytesToReadInCurrentBlock;
        nBytesRead += nBytesToReadInCurrentBlock;
        offset += (FileOffset)nBytesToReadInCurrentBlock;
        dp += nBytesToReadInCurrentBlock;
    }

    *pOutBytesRead = nBytesRead;
    if (*pOutBytesRead > 0 && self->mountFlags.isAccessUpdateOnReadEnabled) {
        Inode_SetModified(pNode, kInodeFlag_Accessed);
    }
    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'pNode' starting at offset 'offset'.
errno_t SerenaFS_xWrite(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset offset, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
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
        AcquireBlock acquireMode = (nBytesToWriteInCurrentBlock == kSFSBlockSize) ? kAcquireBlock_Replace : kAcquireBlock_Update;
        DiskBlockRef pBlock;

        errno_t e1 = SerenaFS_AcquireFileBlock(self, pNode, blockIdx, acquireMode, &pBlock);
        if (e1 == EOK) {
            uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        
            memcpy(bp + blockOffset, ((const uint8_t*) pBuffer) + nBytesWritten, nBytesToWriteInCurrentBlock);
            e1 = FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        }
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
void SerenaFS_xTruncateFile(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pNode, FileOffset newLength)
{
    decl_try_err();
    const FileOffset oldLength = Inode_GetFileSize(pNode);
    SFSBlockNumber* ino_bmap = Inode_GetBlockMap(pNode);
    const SFSBlockNumber bn_nlen = (SFSBlockNumber)(newLength >> (FileOffset)kSFSBlockSizeShift);   //XXX should be 64bit
    const size_t boff_nlen = newLength & (FileOffset)kSFSBlockSizeMask;
    SFSBlockNumber bn_first_to_discard = (boff_nlen > 0) ? bn_nlen + 1 : bn_nlen;   // first block to discard (the block that contains newLength or that is right in front of newLength)

    if (bn_first_to_discard < kSFSDirectBlockPointersCount) {
        for (SFSBlockNumber bn = bn_first_to_discard; bn < kSFSDirectBlockPointersCount; bn++) {
            if (ino_bmap[bn] != 0) {
                SerenaFS_DeallocateBlock(self, UInt32_BigToHost(ino_bmap[bn]));
                ino_bmap[bn] = 0;
            }
        }
    }

    const SFSBlockNumber bn_first_i1_to_discard = (bn_first_to_discard < kSFSDirectBlockPointersCount) ? 0 : bn_first_to_discard - kSFSDirectBlockPointersCount;
    const LogicalBlockAddress i1_lba = UInt32_BigToHost(ino_bmap[kSFSDirectBlockPointersCount]);

    if (i1_lba != 0) {
        FSContainerRef fsContainer = Filesystem_GetContainer(self);
        DiskBlockRef pBlock;

        err = FSContainer_AcquireBlock(fsContainer, i1_lba, kAcquireBlock_Update, &pBlock);
        if (err == EOK) {
            SFSBlockNumber* i1_bmap = (SFSBlockNumber*)DiskBlock_GetMutableData(pBlock);

            for (SFSBlockNumber bn = bn_first_i1_to_discard; bn < kSFSBlockPointersPerBlockCount; bn++) {
                if (i1_bmap[bn] != 0) {
                    SerenaFS_DeallocateBlock(self, UInt32_BigToHost(i1_bmap[bn]));
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


    Inode_SetFileSize(pNode, newLength);
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
