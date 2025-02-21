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

errno_t SfsRegularFile_write(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, const void* _Nonnull buf, ssize_t nBytesToWrite, ssize_t* _Nonnull pOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
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


    while (nBytesToWrite > 0) {
        const int blockIdx = (int)(offset >> (FileOffset)kSFSBlockSizeShift);   //XXX blockIdx should be 64bit
        const size_t blockOffset = offset & (FileOffset)kSFSBlockSizeMask;
        const size_t nBytesToWriteInCurrentBlock = __min(kSFSBlockSize - blockOffset, nBytesToWrite);
        AcquireBlock acquireMode = (nBytesToWriteInCurrentBlock == kSFSBlockSize) ? kAcquireBlock_Replace : kAcquireBlock_Update;
        DiskBlockRef pBlock;

        errno_t e1 = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, acquireMode, &pBlock);
        if (e1 == EOK) {
            uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        
            memcpy(bp + blockOffset, ((const uint8_t*) buf) + nBytesWritten, nBytesToWriteInCurrentBlock);
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
