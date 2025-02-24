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

// Returns true if the given directory node is not empty (contains more than
// just "." and ".." or has a link count > 1).
bool SfsDirectory_IsNotEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetLinkCount(self) > 1 || Inode_GetFileSize(self) > 2 * sizeof(sfs_dirent_t);
}

// Returns true if the array of directory entries starting at 'pEntry' and holding
// 'nEntries' entries contains a directory entry that matches 'pQuery'.
static bool has_matching_dirent(const SFSDirectoryQuery* _Nonnull pq, const sfs_dirent_t* _Nonnull pBlock, int nEntries, sfs_dirent_t* _Nullable * _Nullable pOutEmptyPtr, sfs_dirent_t* _Nullable * _Nonnull pOutEntryPtr)
{
    const sfs_dirent_t* pe = pBlock;

    *pOutEmptyPtr = NULL;
    *pOutEntryPtr = NULL;

    while (nEntries-- > 0) {
        if (pe->id > 0) {
            switch (pq->kind) {
                case kSFSDirectoryQuery_PathComponent:
                    if (PathComponent_EqualsString(pq->u.pc, pe->filename, pe->len)) {
                        *pOutEntryPtr = (sfs_dirent_t*)pe;
                        return true;
                    }
                    break;

                case kSFSDirectoryQuery_InodeId:
                    if (pe->id == pq->u.id) {
                       *pOutEntryPtr = (sfs_dirent_t*)pe;
                        return true;
                    }
                    break;

                default:
                    abort();
            }
        }
        else if (pOutEmptyPtr) {
            *pOutEmptyPtr = (sfs_dirent_t*)pe;
        }
        pe++;
    }

    return false;
}

// Returns a reference to the directory entry that holds 'pName'. NULL and a
// suitable error is returned if no such entry exists or 'pName' is empty or
// too long.
errno_t SfsDirectory_GetEntry(
    InodeRef _Nonnull _Locked self,
    const SFSDirectoryQuery* _Nonnull pQuery,
    sfs_dirent_ptr_t* _Nullable pOutEmptyPtr,
    sfs_dirent_ptr_t* _Nullable pOutEntryPtr,
    InodeId* _Nullable pOutId,
    MutablePathComponent* _Nullable pOutFilename)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const FileOffset fileSize = Inode_GetFileSize(self);
    FileOffset offset = 0ll;
    sfs_dirent_t* pEmptyEntry = NULL;
    sfs_dirent_t* pMatchingEntry = NULL;
    SFSDirectoryQuery swappedQuery;
    bool hasMatch = false;

    if (pOutEmptyPtr) {
        pOutEmptyPtr->lba = 0;
        pOutEmptyPtr->blockOffset = 0;
        pOutEmptyPtr->fileOffset = 0ll;
    }
    if (pOutEntryPtr) {
        pOutEntryPtr->lba = 0;
        pOutEntryPtr->blockOffset = 0;
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

    swappedQuery = *pQuery;
    if (swappedQuery.kind == kSFSDirectoryQuery_InodeId) {
        swappedQuery.u.id = UInt32_HostToBig(swappedQuery.u.id);
    }

    while (true) {
        const sfs_bno_t blockIdx = (sfs_bno_t)(offset >> (FileOffset)kSFSBlockSizeShift);
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)kSFSBlockSize, fileSize - offset);
        DiskBlockRef pBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, kAcquireBlock_ReadOnly, &pBlock));

        const sfs_dirent_t* pDirBuffer = (const sfs_dirent_t*)DiskBlock_GetData(pBlock);
        const int nDirEntries = nBytesAvailable / sizeof(sfs_dirent_t);
        hasMatch = has_matching_dirent(&swappedQuery, pDirBuffer, nDirEntries, &pEmptyEntry, &pMatchingEntry);
        if (pEmptyEntry) {
            pOutEmptyPtr->lba = DiskBlock_GetDiskAddress(pBlock)->lba;
            pOutEmptyPtr->blockOffset = ((uint8_t*)pEmptyEntry) - ((uint8_t*)pDirBuffer);
            pOutEmptyPtr->fileOffset = offset + pOutEmptyPtr->blockOffset;
        }
        if (hasMatch) {
            if (pOutEntryPtr) {
                pOutEntryPtr->lba = DiskBlock_GetDiskAddress(pBlock)->lba;
                pOutEntryPtr->blockOffset = ((uint8_t*)pMatchingEntry) - ((uint8_t*)pDirBuffer);
                pOutEntryPtr->fileOffset = offset + pOutEntryPtr->blockOffset;
            }
            if (pOutId) {
                *pOutId = UInt32_BigToHost(pMatchingEntry->id);
            }
            if (pOutFilename) {
                try(MutablePathComponent_SetString(pOutFilename, pMatchingEntry->filename, pMatchingEntry->len));
            }
        }

        FSContainer_RelinquishBlock(fsContainer, pBlock);
        pBlock = NULL;

        if (hasMatch) {
            break;
        }

        offset += (FileOffset)nBytesAvailable;
    }

    if (!hasMatch) {
        err = ENOENT;
    }

catch:
    return err;
}

errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, InodeId idToRemove)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    sfs_dirent_ptr_t mp;
    SFSDirectoryQuery q;
    DiskBlockRef pBlock;

    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = idToRemove;
    try(SfsDirectory_GetEntry(self, &q, NULL, &mp, NULL, NULL));

    try(FSContainer_AcquireBlock(fsContainer, mp.lba, kAcquireBlock_Update, &pBlock));
    uint8_t* bp = DiskBlock_GetMutableData(pBlock);
    sfs_dirent_t* dep = (sfs_dirent_t*)(bp + mp.blockOffset);
    memset(dep, 0, sizeof(sfs_dirent_t));
    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

    if (Inode_GetFileSize(self) - (FileOffset)sizeof(sfs_dirent_t) == mp.fileOffset) {
        Inode_DecrementFileSize(self, sizeof(sfs_dirent_t));
    }

catch:
    return err;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'self'. 'pEmptyEntry' is an optional insertion hint. If this pointer exists
// then the directory entry that it points to will be reused for the new directory
// entry; otherwise a completely new entry will be added to the directory.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
errno_t SfsDirectory_InsertEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull pName, InodeId id, const sfs_dirent_ptr_t* _Nullable pEmptyPtr)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs); 
    DiskBlockRef pBlock;

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyPtr && pEmptyPtr->lba > 0) {
        // Reuse an empty entry
        try(FSContainer_AcquireBlock(fsContainer, pEmptyPtr->lba, kAcquireBlock_Update, &pBlock));
        uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        sfs_dirent_t* dep = (sfs_dirent_t*)(bp + pEmptyPtr->blockOffset);

        memset(dep->filename, 0, kSFSMaxFilenameLength);
        memcpy(dep->filename, pName->name, pName->count);
        dep->len = pName->count;
        dep->id = UInt32_HostToBig(id);

        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    }
    else {
        // Append a new entry
        sfs_bno_t* ino_bmap = SfsFile_GetBlockMap(self);
        const FileOffset size = Inode_GetFileSize(self);
        const int remainder = size & kSFSBlockSizeMask;
        sfs_dirent_t* dep;
        LogicalBlockAddress lba;
        int idx = -1;

        if (remainder > 0) {
            idx = size / kSFSBlockSize;
            lba = UInt32_BigToHost(ino_bmap[idx]);

            try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Update, &pBlock));
            uint8_t* bp = DiskBlock_GetMutableData(pBlock);
            dep = (sfs_dirent_t*)(bp + remainder);
        }
        else {
            for (int i = 0; i < kSFSDirectBlockPointersCount; i++) {
                if (ino_bmap[i] == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) {
                throw(EIO);
            }

            try(SfsAllocator_Allocate(&fs->blockAllocator, &lba));
            try(SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer));
            ino_bmap[idx] = UInt32_HostToBig(lba);
            
            try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Cleared, &pBlock));
            dep = (sfs_dirent_t*)DiskBlock_GetMutableData(pBlock);
        }

        memset(dep->filename, 0, kSFSMaxFilenameLength);
        memcpy(dep->filename, pName->name, pName->count);
        dep->len = pName->count;
        dep->id = UInt32_HostToBig(id);
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

        Inode_IncrementFileSize(self, sizeof(sfs_dirent_t));
    }


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}


class_func_defs(SfsDirectory, SfsFile,
override_func_def(read, SfsDirectory, Inode)
);
