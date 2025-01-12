//
//  SerenaFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <filesystem/DirectoryChannel.h>
#include <System/ByteOrder.h>
#include <security/SecurityManager.h>


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Inode extensions
////////////////////////////////////////////////////////////////////////////////

// Returns true if the given directory node is not empty (contains more than
// just "." and ".." or has a link count > 1).
bool DirectoryNode_IsNotEmpty(InodeRef _Nonnull _Locked self)
{
    return Inode_GetLinkCount(self) > 1 || Inode_GetFileSize(self) > 2 * sizeof(SFSDirectoryEntry);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Filesystem
////////////////////////////////////////////////////////////////////////////////

// Returns true if the array of directory entries starting at 'pEntry' and holding
// 'nEntries' entries contains a directory entry that matches 'pQuery'.
static bool xHasMatchingDirectoryEntry(const SFSDirectoryQuery* _Nonnull pQuery, const SFSDirectoryEntry* _Nonnull pBlock, int nEntries, SFSDirectoryEntry* _Nullable * _Nullable pOutEmptyPtr, SFSDirectoryEntry* _Nullable * _Nonnull pOutEntryPtr)
{
    const SFSDirectoryEntry* pEntry = pBlock;

    *pOutEmptyPtr = NULL;
    *pOutEntryPtr = NULL;

    while (nEntries-- > 0) {
        if (pEntry->id > 0) {
            switch (pQuery->kind) {
                case kSFSDirectoryQuery_PathComponent:
                    if (PathComponent_EqualsCString(pQuery->u.pc, pEntry->filename)) {
                        *pOutEntryPtr = (SFSDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                case kSFSDirectoryQuery_InodeId:
                    if (pEntry->id == pQuery->u.id) {
                       *pOutEntryPtr = (SFSDirectoryEntry*)pEntry;
                        return true;
                    }
                    break;

                default:
                    abort();
            }
        }
        else if (pOutEmptyPtr) {
            *pOutEmptyPtr = (SFSDirectoryEntry*)pEntry;
        }
        pEntry++;
    }

    return false;
}

// Returns a reference to the directory entry that holds 'pName'. NULL and a
// suitable error is returned if no such entry exists or 'pName' is empty or
// too long.
errno_t SerenaFS_GetDirectoryEntry(
    SerenaFSRef _Nonnull self,
    InodeRef _Nonnull _Locked pNode,
    const SFSDirectoryQuery* _Nonnull pQuery,
    SFSDirectoryEntryPointer* _Nullable pOutEmptyPtr,
    SFSDirectoryEntryPointer* _Nullable pOutEntryPtr,
    InodeId* _Nullable pOutId,
    MutablePathComponent* _Nullable pOutFilename)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    FileOffset offset = 0ll;
    SFSDirectoryEntry* pEmptyEntry = NULL;
    SFSDirectoryEntry* pMatchingEntry = NULL;
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
        const int blockIdx = offset >> (FileOffset)kSFSBlockSizeShift;
        const ssize_t nBytesAvailable = (ssize_t)__min((FileOffset)kSFSBlockSize, fileSize - offset);
        DiskBlockRef pBlock;

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SerenaFS_AcquireFileBlock(self, pNode, blockIdx, kAcquireBlock_ReadOnly, &pBlock));

        const SFSDirectoryEntry* pDirBuffer = (const SFSDirectoryEntry*)DiskBlock_GetData(pBlock);
        const int nDirEntries = nBytesAvailable / sizeof(SFSDirectoryEntry);
        hasMatch = xHasMatchingDirectoryEntry(&swappedQuery, pDirBuffer, nDirEntries, &pEmptyEntry, &pMatchingEntry);
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
                const ssize_t len = String_LengthUpTo(pMatchingEntry->filename, kSFSMaxFilenameLength);
                if (len > pOutFilename->capacity) {
                    throw(ERANGE);
                }

                String_CopyUpTo(pOutFilename->name, pMatchingEntry->filename, len);
                pOutFilename->count = len;
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

errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    err = SELock_LockShared(&self->seLock);
    if (err == EOK) {
        if (self->mountFlags.isMounted) {
            err = Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirLba, pOutDir);
        }
        else {
            err = EIO;
        }
        SELock_Unlock(&self->seLock);
    }
    return err;
}

errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, UserId uid, GroupId gid, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    SFSDirectoryQuery q;
    InodeId entryId;

    try(SecurityManager_CheckNodeAccess(gSecurityManager, pDir, uid, gid, kAccess_Searchable));
    q.kind = kSFSDirectoryQuery_PathComponent;
    q.u.pc = pName;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, (pDirInsHint) ? (SFSDirectoryEntryPointer*)pDirInsHint->data : NULL, NULL, &entryId, NULL));
    if (pOutNode) {
        try(Filesystem_AcquireNodeWithId((FilesystemRef)self, entryId, pOutNode));
    }
    return EOK;

catch:
    if (pOutNode) {
        *pOutNode = NULL;
    }
    return err;
}

errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, UserId uid, GroupId gid, MutablePathComponent* _Nonnull pName)
{
    decl_try_err();
    SFSDirectoryQuery q;

    try(SecurityManager_CheckNodeAccess(gSecurityManager, pDir, uid, gid, kAccess_Readable | kAccess_Searchable));
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = id;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, NULL, NULL, NULL, pName));
    return EOK;

catch:
    pName->count = 0;
    return err;
}

errno_t SerenaFS_RemoveDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId idToRemove)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self);
    SFSDirectoryEntryPointer mp;
    SFSDirectoryQuery q;
    DiskBlockRef pBlock;

    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = idToRemove;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, NULL, &mp, NULL, NULL));

    try(FSContainer_AcquireBlock(fsContainer, mp.lba, kAcquireBlock_Update, &pBlock));
    uint8_t* bp = DiskBlock_GetMutableData(pBlock);
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(bp + mp.blockOffset);
    memset(dep, 0, sizeof(SFSDirectoryEntry));
    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

    if (Inode_GetFileSize(pDir) - (FileOffset)sizeof(SFSDirectoryEntry) == mp.fileOffset) {
        Inode_DecrementFileSize(pDir, sizeof(SFSDirectoryEntry));
    }

catch:
    return err;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'pDirNode'. 'pEmptyEntry' is an optional insertion hint. If this pointer exists
// then the directory entry that it points to will be reused for the new directory
// entry; otherwise a completely new entry will be added to the directory.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
errno_t SerenaFS_InsertDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, const PathComponent* _Nonnull pName, InodeId id, const SFSDirectoryEntryPointer* _Nullable pEmptyPtr)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(self); 
    DiskBlockRef pBlock;

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyPtr && pEmptyPtr->lba > 0) {
        // Reuse an empty entry
        try(FSContainer_AcquireBlock(fsContainer, pEmptyPtr->lba, kAcquireBlock_Update, &pBlock));
        uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(bp + pEmptyPtr->blockOffset);

        char* p = String_CopyUpTo(dep->filename, pName->name, pName->count);
        while (p < &dep->filename[kSFSMaxFilenameLength]) *p++ = '\0';
        dep->id = UInt32_HostToBig(id);

        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    }
    else {
        // Append a new entry
        SFSBlockNumber* ino_bmap = Inode_GetBlockMap(pDirNode);
        const FileOffset size = Inode_GetFileSize(pDirNode);
        const int remainder = size & kSFSBlockSizeMask;
        SFSDirectoryEntry* dep;
        LogicalBlockAddress lba;
        int idx = -1;

        if (remainder > 0) {
            idx = size / kSFSBlockSize;
            lba = UInt32_BigToHost(ino_bmap[idx]);

            try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Update, &pBlock));
            uint8_t* bp = DiskBlock_GetMutableData(pBlock);
            dep = (SFSDirectoryEntry*)(bp + remainder);
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

            try(BlockAllocator_Allocate(&self->blockAllocator, &lba));
            try(BlockAllocator_CommitToDisk(&self->blockAllocator, fsContainer));
            ino_bmap[idx] = UInt32_HostToBig(lba);
            
            try(FSContainer_AcquireBlock(fsContainer, lba, kAcquireBlock_Cleared, &pBlock));
            dep = (SFSDirectoryEntry*)DiskBlock_GetMutableData(pBlock);
        }

        String_CopyUpTo(dep->filename, pName->name, pName->count);
        dep->id = UInt32_HostToBig(id);
        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

        Inode_IncrementFileSize(pDirNode, sizeof(SFSDirectoryEntry));
    }


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDirNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, DirectoryChannelRef _Nonnull _Locked pChannel, void* _Nonnull pBuffer, ssize_t nBytesToRead, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    FileOffset offset = IOChannel_GetOffset(pChannel);
    SFSDirectoryEntry dirent;
    ssize_t nAllDirBytesRead = 0;
    ssize_t nBytesRead = 0;

    while (nBytesToRead > 0) {
        ssize_t nDirBytesRead;
        const errno_t e1 = SerenaFS_xRead(self, DirectoryChannel_GetInode(pChannel), offset, &dirent, sizeof(SFSDirectoryEntry), &nDirBytesRead);

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
        IOChannel_IncrementOffsetBy(pChannel, nAllDirBytesRead);
    }
    *nOutBytesRead = nBytesRead;

    return err;
}
