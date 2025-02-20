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
#include <kobj/AnyRefs.h>


errno_t SfsRegularFile_read(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);

    err = SfsFile_xRead((SfsFileRef)self, IOChannel_GetOffset(ch), pBuffer, nBytesToRead, nOutBytesRead);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesRead);

    return err;
}

// Writes 'nBytesToWrite' bytes to the file 'self' starting at offset 'offset'.
static errno_t write_contents(SfsRegularFileRef _Nonnull _Locked self, FileOffset offset, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
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

        errno_t e1 = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, acquireMode, &pBlock);
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

    const errno_t e2 = SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer);
    if (err == EOK) {
        err = e2;
    }

    if (nBytesWritten > 0) {
        if (offset > Inode_GetFileSize(self)) {
            Inode_SetFileSize(self, offset);
        }
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }
    *pOutBytesWritten = nBytesWritten;
    return err;
}

errno_t SfsRegularFile_write(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FileOffset offset;

    if ((IOChannel_GetMode(ch) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self);
    }
    else {
        offset = IOChannel_GetOffset(ch);
    }

    err = write_contents(self, offset, pBuffer, nBytesToWrite, nOutBytesWritten);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesWritten);

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
