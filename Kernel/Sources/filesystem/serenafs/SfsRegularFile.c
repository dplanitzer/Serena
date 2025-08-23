//
//  SfsRegularFile.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsRegularFile.h"
#include "SerenaFSPriv.h"
#include <filesystem/InodeChannel.h>
#include <kern/limits.h>
#include <kern/string.h>
#include <kpi/fcntl.h>


errno_t SfsRegularFile_read(SfsRegularFileRef _Nonnull _Locked self, InodeChannelRef _Nonnull _Locked ch, void* _Nonnull buf, ssize_t nBytesToRead, ssize_t* _Nonnull pOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    const off_t offset = IOChannel_GetOffset(ch);
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
    const off_t nAvailBytes = Inode_GetFileSize(self) - offset;

    if (nAvailBytes > 0) {
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToRead) {
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
        SfsFileBlock blk;

        const errno_t e1 = SfsFile_MapBlock((SfsFileRef)self, blockIdx, kMapBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        memcpy(dp, blk.b.data + blockOffset, nBytesToReadInBlock);
        SfsFile_UnmapBlock((SfsFileRef)self, &blk, kWriteBlock_None);

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

#ifdef _WIN32
#define MAX_FILE_SIZE   LONG_MAX
#else
#define MAX_FILE_SIZE   kSFSLimit_FileSizeMax
#endif

errno_t SfsRegularFile_write(SfsRegularFileRef _Nonnull _Locked self, InodeChannelRef _Nonnull _Locked ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const uint8_t* sp = buf;
    ssize_t nBytesWritten = 0;
    off_t offset;

    if ((IOChannel_GetMode(ch) & O_APPEND) == O_APPEND) {
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
        if (offset >= MAX_FILE_SIZE) {
            throw(EFBIG);
        }

        const off_t nAvailBytes = MAX_FILE_SIZE - offset;
        if (nAvailBytes <= (off_t)SSIZE_MAX && (ssize_t)nAvailBytes < nBytesToWrite) {
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
        MapBlock mmode = (nBytesToWriteInBlock == fs->blockAllocator.blockSize) ? kMapBlock_Replace : kMapBlock_Update;
        SfsFileBlock blk;

        errno_t e1 = SfsFile_MapBlock((SfsFileRef)self, blockIdx, mmode, &blk);
        if (e1 == EOK) {
            memcpy(blk.b.data + blockOffset, sp, nBytesToWriteInBlock);
            e1 = SfsFile_UnmapBlock((SfsFileRef)self, &blk, kWriteBlock_Deferred);
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
        const off_t endOffset = offset + (off_t)nBytesWritten;

        if (endOffset > Inode_GetFileSize(self)) {
            Inode_SetFileSize(self, endOffset);
        }
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
        Inode_Writeback((InodeRef)self);
        IOChannel_IncrementOffsetBy(ch, nBytesWritten);
    }


catch:
    *pOutBytesWritten = nBytesWritten;
    return err;
}

errno_t SfsRegularFile_truncate(SfsRegularFileRef _Nonnull _Locked self, off_t length)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);

    const off_t oldLength = Inode_GetFileSize(self);
    if (oldLength < length) {
        // Expansion in size
        // Just set the new file size. The needed blocks will be allocated on
        // demand when read/write is called to manipulate the new data range.
        Inode_SetFileSize(self, length);
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged); 
    }
    else if (oldLength > length) {
        // Reduction in size
        SfsFile_Trim((SfsFileRef)self, length);
        SfsAllocator_CommitToDisk(&fs->blockAllocator, Filesystem_GetContainer(fs));

        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);    
    }

    Inode_Writeback((InodeRef)self);
    
    return err;
}


class_func_defs(SfsRegularFile, SfsFile,
override_func_def(read, SfsRegularFile, Inode)
override_func_def(write, SfsRegularFile, Inode)
override_func_def(truncate, SfsRegularFile, Inode)    
);
