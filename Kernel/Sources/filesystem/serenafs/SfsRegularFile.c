//
//  SfsRegularFile.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsRegularFile.h"
#include "SerenaFSPriv.h"
#include <filesystem/FileChannel.h>


errno_t SfsRegularFile_read(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const FileOffset offset = IOChannel_GetOffset(ch);
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
    const FileOffset nAvailBytes = Inode_GetFileSize(self) - offset;

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
    SfsFile_ConvertOffset((SfsFileRef)self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    while (nBytesToRead > 0) {
        const ssize_t nRemainderBlockSize = fs->blockAllocator.blockSize - blockOffset;
        const ssize_t nBytesToReadInBlock = (nBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToRead;
        DiskBlockRef pBlock;

        const errno_t e1 = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
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


    if (nBytesRead > 0) {
        if (fs->mountFlags.isAccessUpdateOnReadEnabled) {
            Inode_SetModified(self, kInodeFlag_Accessed);
        }
        IOChannel_IncrementOffsetBy(ch, nBytesRead);
    }

catch:
    *pOutBytesRead = nBytesRead;
    return err;
}

errno_t SfsRegularFile_write(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const uint8_t* sp = buf;
    ssize_t nBytesWritten = 0;
    FileOffset offset;

    if ((IOChannel_GetMode(ch) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self);
    }
    else {
        offset = IOChannel_GetOffset(ch);
    }


    if (nBytesToWrite < 0) {
        throw(EINVAL);
    }
    if (nBytesToWrite > 0 && offset < 0ll) {
        throw(EOVERFLOW);
    }


    // Limit 'nBytesToWrite' to the maximum possible file size relative to 'offset'.
    if (nBytesToWrite > 0) {
        if (offset >= kSFSLimit_FileSizeMax) {
            throw(EFBIG);
        }

        const FileOffset nAvailBytes = kSFSLimit_FileSizeMax - offset;
        if (nAvailBytes <= (FileOffset)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToWrite) {
            nBytesToWrite = (ssize_t)nAvailBytes;
        }
        // Otherwise, use 'nBytesToWrite' as is
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    sfs_bno_t blockIdx;
    ssize_t blockOffset;
    SfsFile_ConvertOffset((SfsFileRef)self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've written all
    // required bytes.
    while (nBytesToWrite > 0) {
        const ssize_t nRemainderBlockSize = fs->blockAllocator.blockSize - blockOffset;
        const ssize_t nBytesToWriteInBlock = (nBytesToWrite > nRemainderBlockSize) ? nRemainderBlockSize : nBytesToWrite;
        AcquireBlock acquireMode = (nBytesToWriteInBlock == fs->blockAllocator.blockSize) ? kAcquireBlock_Replace : kAcquireBlock_Update;
        DiskBlockRef pBlock;

        errno_t e1 = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, acquireMode, &pBlock);
        if (e1 == EOK) {
            uint8_t* dp = DiskBlock_GetMutableData(pBlock);
        
            memcpy(dp + blockOffset, sp, nBytesToWriteInBlock);
            e1 = FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
        }
        if (e1 != EOK) {
            err = (nBytesWritten == 0) ? e1 : EOK;
            break;
        }

        nBytesToWrite -= nBytesToWriteInBlock;
        nBytesWritten += nBytesToWriteInBlock;
        sp += nBytesToWriteInBlock;

        blockOffset = 0;
        blockIdx++;
    }


    const errno_t e2 = SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer);
    if (err == EOK) {
        err = e2;
    }


    if (nBytesWritten > 0) {
        const FileOffset endOffset = offset + (FileOffset)nBytesWritten;

        if (endOffset > Inode_GetFileSize(self)) {
            Inode_SetFileSize(self, endOffset);
        }
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
        IOChannel_IncrementOffsetBy(ch, nBytesWritten);
    }


catch:
    *pOutBytesWritten = nBytesWritten;
    return err;
}

errno_t SfsRegularFile_truncate(SfsRegularFileRef _Nonnull _Locked self, FileOffset length)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);

    const FileOffset oldLength = Inode_GetFileSize(self);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(self, length);
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SfsFile_xTruncate((SfsFileRef)self, length);
    }

    return err;
}


class_func_defs(SfsRegularFile, SfsFile,
override_func_def(read, SfsRegularFile, Inode)
override_func_def(write, SfsRegularFile, Inode)
override_func_def(truncate, SfsRegularFile, Inode)    
);
