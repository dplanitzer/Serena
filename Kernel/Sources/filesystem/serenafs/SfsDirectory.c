//
//  SfsDirectory.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/18/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsDirectory.h"
#include "SerenaFSPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <filesystem/FSUtilities.h>
#include <kobj/AnyRefs.h>
#include <System/ByteOrder.h>


// Reads the next set of directory entries. The first entry read is the one
// at the current directory index stored in 'channel'. This function guarantees
// that it will only ever return complete directories entries. It will never
// return a partial entry. Consequently the provided buffer must be big enough
// to hold at least one directory entry. Note that this function is expected
// to return "." for the entry at index #0 and ".." for the entry at index #1.
errno_t SfsDirectory_read(SfsDirectoryRef _Nonnull _Locked self, DirectoryChannelRef _Nonnull _Locked ch, void* _Nonnull buf, ssize_t nDstBytesToRead, ssize_t* _Nonnull pOutDstBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    FileOffset offset = IOChannel_GetOffset(ch);
    DirectoryEntry* dp = buf;
    ssize_t nSrcBytesRead = 0;
    ssize_t nDstBytesRead = 0;

    if (nDstBytesToRead < 0) {
        throw(EINVAL);
    }
    if (nDstBytesToRead > 0 && offset < 0ll) {
        throw(EOVERFLOW);
    }


    // Calculate the amount of bytes available in the directory starting at
    // offset 'offset'.
    const FileOffset nAvailBytes = Inode_GetFileSize(self) - offset;
    ssize_t nSrcBytesToRead;

    if (nAvailBytes <= (FileOffset)SSIZE_MAX) {
        nSrcBytesToRead = (ssize_t)nAvailBytes;
    } else {
        nSrcBytesToRead = SSIZE_MAX;
    }


    // Get the block index and block offset that corresponds to 'offset'. We start
    // iterating through blocks there.
    sfs_bno_t blockIdx;
    ssize_t blockOffset;
    SfsFile_ConvertOffset((SfsFileRef)self, offset, &blockIdx, &blockOffset);


    // Iterate through a contiguous sequence of blocks until we've read all
    // required bytes.
    while (nSrcBytesToRead > 0 && nDstBytesToRead >= sizeof(DirectoryEntry)) {
        const ssize_t nRemainderBlockSize = kSFSBlockSize - blockOffset;
        ssize_t nBytesToReadInBlock = (nSrcBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nSrcBytesToRead;
        DiskBlockRef pBlock;

        const errno_t e1 = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
        if (e1 != EOK) {
            err = (nSrcBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        const uint8_t* bp = DiskBlock_GetData(pBlock);
        const sfs_dirent_t* sp = (const sfs_dirent_t*)(bp + blockOffset);
        while (nBytesToReadInBlock > 0 && nDstBytesToRead >= sizeof(DirectoryEntry)) {
            if (sp->id > 0) {
                dp->inodeId = UInt32_BigToHost(sp->id);
                memcpy(dp->name, sp->filename, sp->len);
                dp->name[sp->len] = '\0';
                
                nDstBytesRead += sizeof(DirectoryEntry);
                nDstBytesToRead -= sizeof(DirectoryEntry);
                dp++;
            }
            
            nBytesToReadInBlock -= sizeof(sfs_dirent_t);
            nSrcBytesToRead -= sizeof(sfs_dirent_t);
            nSrcBytesRead += sizeof(sfs_dirent_t);
            sp++;
        }
        FSContainer_RelinquishBlock(fsContainer, pBlock);

        blockOffset = 0;
        blockIdx++;
    }


    if (nSrcBytesRead > 0 && fs->mountFlags.isAccessUpdateOnReadEnabled) {
        Inode_SetModified(self, kInodeFlag_Accessed);
    }
    IOChannel_IncrementOffsetBy(ch, nSrcBytesRead);

catch:
    *pOutDstBytesRead = nDstBytesRead;
    return err;
}


class_func_defs(SfsDirectory, SfsFile,
override_func_def(read, SfsDirectory, Inode)
);
