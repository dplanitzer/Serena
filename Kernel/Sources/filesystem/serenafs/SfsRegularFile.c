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
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);

    err = SerenaFS_xRead(fs, (InodeRef)self, IOChannel_GetOffset(ch), pBuffer, nBytesToRead, nOutBytesRead);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesRead);

    return err;
}

errno_t SfsRegularFile_write(SfsRegularFileRef _Nonnull _Locked self, FileChannelRef _Nonnull _Locked ch, const void* _Nonnull pBuffer, ssize_t nBytesToWrite, ssize_t* _Nonnull nOutBytesWritten)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);
    FileOffset offset;

    if ((IOChannel_GetMode(ch) & kOpen_Append) == kOpen_Append) {
        offset = Inode_GetFileSize(self);
    }
    else {
        offset = IOChannel_GetOffset(ch);
    }

    err = SerenaFS_xWrite(fs, (InodeRef)self, offset, pBuffer, nBytesToWrite, nOutBytesWritten);
    IOChannel_IncrementOffsetBy(ch, *nOutBytesWritten);

    return err;
}

errno_t SfsRegularFile_truncate(SfsRegularFileRef _Nonnull _Locked self, FileOffset length)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);

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
        SerenaFS_xTruncateFile(fs, (InodeRef)self, length);
    }

    return err;
}


class_func_defs(SfsRegularFile, SfsFile,
override_func_def(read, SfsRegularFile, Inode)
override_func_def(write, SfsRegularFile, Inode)
override_func_def(truncate, SfsRegularFile, Inode)    
);
