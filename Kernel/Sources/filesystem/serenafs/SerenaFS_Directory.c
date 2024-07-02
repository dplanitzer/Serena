//
//  SerenaFS_Directory.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/11/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "SerenaFSPriv.h"
#include <System/ByteOrder.h>


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
                    if (PathComponent_EqualsString(pQuery->u.pc, pEntry->filename)) {
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
    const SFSDirectoryEntry* pDirBuffer = (const SFSDirectoryEntry*)self->tmpBlock;
    const FileOffset fileSize = Inode_GetFileSize(pNode);
    FileOffset offset = 0ll;
    LogicalBlockAddress lba = 0;
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

        if (nBytesAvailable <= 0) {
            break;
        }

        try(SerenaFS_GetLogicalBlockAddressForFileBlockAddress(self, pNode, blockIdx, kSFSBlockMode_Read, &lba));
        if (lba == 0) {
            memset(self->tmpBlock, 0, kSFSBlockSize);
        }
        else {
            try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
        }

        const int nDirEntries = nBytesAvailable / sizeof(SFSDirectoryEntry);
        hasMatch = xHasMatchingDirectoryEntry(&swappedQuery, pDirBuffer, nDirEntries, &pEmptyEntry, &pMatchingEntry);
        if (pEmptyEntry) {
            pOutEmptyPtr->lba = lba;
            pOutEmptyPtr->blockOffset = ((uint8_t*)pEmptyEntry) - ((uint8_t*)pDirBuffer);
            pOutEmptyPtr->fileOffset = offset + pOutEmptyPtr->blockOffset;
        }
        if (hasMatch) {
            break;
        }

        offset += (FileOffset)nBytesAvailable;
    }

    if (hasMatch) {
        if (pOutEntryPtr) {
            pOutEntryPtr->lba = lba;
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
        return EOK;
    }
    else {
        return ENOENT;
    }

catch:
    return err;
}

errno_t SerenaFS_acquireRootDirectory(SerenaFSRef _Nonnull self, InodeRef _Nullable * _Nonnull pOutDir)
{
    decl_try_err();

    err = SELock_LockShared(&self->seLock);
    if (err == EOK) {
        if (self->flags.isMounted) {
            err = Filesystem_AcquireNodeWithId((FilesystemRef)self, self->rootDirLba, pOutDir);
        }
        else {
            err = EIO;
        }
        SELock_Unlock(&self->seLock);
    }
    return err;
}

errno_t SerenaFS_acquireNodeForName(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, const PathComponent* _Nonnull pName, User user, DirectoryEntryInsertionHint* _Nullable pDirInsHint, InodeRef _Nullable * _Nullable pOutNode)
{
    decl_try_err();
    SFSDirectoryQuery q;
    InodeId entryId;

    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Searchable));
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

errno_t SerenaFS_getNameOfNode(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, InodeId id, User user, MutablePathComponent* _Nonnull pName)
{
    decl_try_err();
    SFSDirectoryQuery q;

    try(Filesystem_CheckAccess(self, pDir, user, kAccess_Readable | kAccess_Searchable));
    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = id;
    try(SerenaFS_GetDirectoryEntry(self, pDir, &q, NULL, NULL, NULL, pName));
    return EOK;

catch:
    pName->count = 0;
    return err;
}

errno_t SerenaFS_RemoveDirectoryEntry(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDirNode, InodeId idToRemove)
{
    decl_try_err();
    SFSDirectoryEntryPointer mp;
    SFSDirectoryQuery q;

    q.kind = kSFSDirectoryQuery_InodeId;
    q.u.id = idToRemove;
    try(SerenaFS_GetDirectoryEntry(self, pDirNode, &q, NULL, &mp, NULL, NULL));

    try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, mp.lba));
    SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + mp.blockOffset);
    memset(dep, 0, sizeof(SFSDirectoryEntry));
    try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, mp.lba));

    if (Inode_GetFileSize(pDirNode) - (FileOffset)sizeof(SFSDirectoryEntry) == mp.fileOffset) {
        Inode_DecrementFileSize(pDirNode, sizeof(SFSDirectoryEntry));
    }

    return EOK;

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

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (pEmptyPtr && pEmptyPtr->lba > 0) {
        // Reuse an empty entry
        try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, pEmptyPtr->lba));
        SFSDirectoryEntry* dep = (SFSDirectoryEntry*)(self->tmpBlock + pEmptyPtr->blockOffset);

        char* p = String_CopyUpTo(dep->filename, pName->name, pName->count);
        while (p < &dep->filename[kSFSMaxFilenameLength]) *p++ = '\0';
        dep->id = UInt32_HostToBig(id);

        try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, pEmptyPtr->lba));
    }
    else {
        // Append a new entry
        SFSBlockNumber* ino_bp = Inode_GetBlockMap(pDirNode);
        const FileOffset size = Inode_GetFileSize(pDirNode);
        const int remainder = size & kSFSBlockSizeMask;
        SFSDirectoryEntry* dep;
        LogicalBlockAddress lba;
        int idx = -1;

        if (remainder > 0) {
            idx = size / kSFSBlockSize;
            lba = ino_bp[idx];

            try(DiskDriver_GetBlock(self->diskDriver, self->tmpBlock, lba));
            dep = (SFSDirectoryEntry*)(self->tmpBlock + remainder);
        }
        else {
            for (int i = 0; i < kSFSDirectBlockPointersCount; i++) {
                if (ino_bp[i] == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) {
                throw(EIO);
            }

            try(SerenaFS_AllocateBlock(self, &lba));
            memset(self->tmpBlock, 0, kSFSBlockSize);
            dep = (SFSDirectoryEntry*)self->tmpBlock;
        }

        String_CopyUpTo(dep->filename, pName->name, pName->count);
        dep->id = UInt32_HostToBig(id);
        try(DiskDriver_PutBlock(self->diskDriver, self->tmpBlock, lba));
        ino_bp[idx] = lba;

        Inode_IncrementFileSize(pDirNode, sizeof(SFSDirectoryEntry));
    }


    // Mark the directory as modified
    if (err == EOK) {
        Inode_SetModified(pDirNode, kInodeFlag_Updated | kInodeFlag_StatusChanged);
    }

catch:
    return err;
}

errno_t SerenaFS_openDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, User user)
{
    return Filesystem_CheckAccess(self, pDir, user, kAccess_Readable);
}

errno_t SerenaFS_readDirectory(SerenaFSRef _Nonnull self, InodeRef _Nonnull _Locked pDir, void* _Nonnull pBuffer, ssize_t nBytesToRead, FileOffset* _Nonnull pInOutOffset, ssize_t* _Nonnull nOutBytesRead)
{
    decl_try_err();
    FileOffset offset = *pInOutOffset;
    SFSDirectoryEntry dirent;
    ssize_t nAllDirBytesRead = 0;
    ssize_t nBytesRead = 0;

    while (nBytesToRead > 0) {
        ssize_t nDirBytesRead;
        const errno_t e1 = SerenaFS_xRead(self, pDir, offset, &dirent, sizeof(SFSDirectoryEntry), &nDirBytesRead);

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
        *pInOutOffset += nAllDirBytesRead;
    }
    *nOutBytesRead = nBytesRead;

    return err;
}
