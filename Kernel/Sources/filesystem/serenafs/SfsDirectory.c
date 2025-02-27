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
    off_t offset = IOChannel_GetOffset(ch);
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
    const off_t nAvailBytes = Inode_GetFileSize(self) - offset;
    ssize_t nSrcBytesToRead;

    if (nAvailBytes <= (off_t)SSIZE_MAX) {
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
        const ssize_t nRemainderBlockSize = fs->blockAllocator.blockSize - blockOffset;
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

errno_t SfsDirectory_Query(InodeRef _Nonnull _Locked self, sfs_query_t* _Nonnull q, sfs_query_result_t* _Nonnull qr)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    off_t offset = 0ll;
    const off_t fileSize = Inode_GetFileSize(self);
    sfs_bno_t blockIdx = 0;
    bool hasInsertionHint = false;
    bool done = false;

    qr->id = 0;
    if (q->mpc && q->kind == kSFSQuery_InodeId) {
        qr->mpc = q->mpc;
        qr->mpc->count = 0;
    } else {
        qr->mpc = NULL;
    }
    qr->lba = 0;
    qr->blockOffset = 0;
    qr->fileOffset = 0ll;
    if (q->ih) {
        qr->ih = q->ih;
        qr->ih->lba = 0;
        qr->ih->blockOffset = 0;
    } else {
        qr->ih = NULL;
    }


    if (q->kind == kSFSQuery_PathComponent) {
        if (q->u.pc->count == 0) {
            return ENOENT;
        }
        if (q->u.pc->count > kSFSMaxFilenameLength) {
            return ENAMETOOLONG;
        }
    }


    // Iterate through a contiguous sequence of blocks until we find the desired
    // directory entry.
    while (!done && offset < fileSize) {
        DiskBlockRef pBlock;
        
        err = SfsFile_AcquireBlock((SfsFileRef)self, blockIdx, kAcquireBlock_ReadOnly, &pBlock);
        if (err != EOK) {
            break;
        }
        
        const uint8_t* bp = DiskBlock_GetData(pBlock);
        const sfs_dirent_t* sp = (const sfs_dirent_t*)bp;
        const sfs_dirent_t* ep = (const sfs_dirent_t*)(bp + DiskBlock_GetByteSize(pBlock));

        while (sp < ep && offset < fileSize) {
            if (sp->id > 0) {
                switch (q->kind) {
                    case kSFSQuery_PathComponent:
                        done = PathComponent_EqualsString(q->u.pc, sp->filename, sp->len);
                        break;

                    case kSFSQuery_InodeId:
                        done = (UInt32_HostToBig(q->u.id) == sp->id) ? true : false;
                        break;

                    default:
                        abort();
                        break;
                }
            }
            else if (qr->ih && !hasInsertionHint) {
                qr->ih->lba = blockIdx;
                qr->ih->blockOffset = (const uint8_t*)sp - bp;
                hasInsertionHint = true;
            }
            
            if (done) {
                qr->id = UInt32_BigToHost(sp->id);
                if (qr->mpc) {
                    MutablePathComponent_SetString(qr->mpc, sp->filename, sp->len);
                }
                qr->lba = blockIdx;
                qr->blockOffset = (const uint8_t*)sp - bp;
                qr->fileOffset = offset;
                break;
            }

            offset += sizeof(sfs_dirent_t);
            sp++;
        }
        FSContainer_RelinquishBlock(fsContainer, pBlock);

        blockIdx++;
    }

    return (done) ? err : ENOENT;
}

errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, InodeId idToRemove)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    DiskBlockRef pBlock;
    sfs_query_t q;
    sfs_query_result_t qr;

    q.kind = kSFSQuery_InodeId;
    q.u.id = idToRemove;
    q.mpc = NULL;
    q.ih = NULL;
    try(SfsDirectory_Query(self, &q, &qr));

    try(FSContainer_AcquireBlock(fsContainer, qr.lba, kAcquireBlock_Update, &pBlock));
    uint8_t* bp = DiskBlock_GetMutableData(pBlock);
    sfs_dirent_t* dep = (sfs_dirent_t*)(bp + qr.blockOffset);
    memset(dep, 0, sizeof(sfs_dirent_t));
    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);

    if (Inode_GetFileSize(self) - (off_t)sizeof(sfs_dirent_t) == qr.fileOffset) {
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
errno_t SfsDirectory_InsertEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull pName, InodeId id, const sfs_insertion_hint_t* _Nullable ih)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs); 
    DiskBlockRef pBlock;

    if (pName->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (ih && ih->lba > 0) {
        // Reuse an empty entry
        try(FSContainer_AcquireBlock(fsContainer, ih->lba, kAcquireBlock_Update, &pBlock));
        uint8_t* bp = DiskBlock_GetMutableData(pBlock);
        sfs_dirent_t* dep = (sfs_dirent_t*)(bp + ih->blockOffset);

        memset(dep->filename, 0, kSFSMaxFilenameLength);
        memcpy(dep->filename, pName->name, pName->count);
        dep->len = pName->count;
        dep->id = UInt32_HostToBig(id);

        FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);
    }
    else {
        // Append a new entry
        sfs_bno_t* ino_bmap = SfsFile_GetBlockMap(self);
        const off_t dirSize = Inode_GetFileSize(self);
        const size_t remainder = (size_t)(dirSize & (off_t)fs->blockMask);
        LogicalBlockAddress lba;
        sfs_dirent_t* dep;
        int idx = -1;

        if (remainder > 0) {
            idx = (int)(dirSize >> (off_t)fs->blockShift);
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
