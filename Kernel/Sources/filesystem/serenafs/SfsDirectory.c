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
errno_t SfsDirectory_read(SfsDirectoryRef _Nonnull _Locked self, DirectoryChannelRef _Nonnull _Locked ch, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    SerenaFSRef fs = (SerenaFSRef)Inode_GetFilesystem(self);
    FileOffset offset = IOChannel_GetOffset(ch);
    sfs_dirent_t dirent;
    ssize_t nAllDirBytesRead = 0;
    ssize_t nBytesRead = 0;

    while (nBytesToRead > 0) {
        ssize_t nDirBytesRead;
        const errno_t e1 = SfsFile_xRead((SfsFileRef)self, offset, &dirent, sizeof(sfs_dirent_t), &nDirBytesRead);

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
        IOChannel_IncrementOffsetBy(ch, nAllDirBytesRead);
    }
    *nOutBytesRead = nBytesRead;

    return err;
}


class_func_defs(SfsDirectory, SfsFile,
override_func_def(read, SfsDirectory, Inode)
);
