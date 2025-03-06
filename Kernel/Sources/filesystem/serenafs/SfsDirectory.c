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
                dp->inid = UInt32_BigToHost(sp->id);
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
                qr->ih->lba = DiskBlock_GetDiskAddress(pBlock)->lba;
                qr->ih->blockOffset = (const uint8_t*)sp - bp;
                hasInsertionHint = true;
            }
            
            if (done) {
                qr->id = UInt32_BigToHost(sp->id);
                if (qr->mpc) {
                    MutablePathComponent_SetString(qr->mpc, sp->filename, sp->len);
                }
                qr->lba = DiskBlock_GetDiskAddress(pBlock)->lba;
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

errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, ino_t idToRemove)
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
        Inode_SetFileSize(self, qr.fileOffset);
    }

catch:
    return err;
}

// Validates that adding an entry with name 'name' and file type 'type' to this
// directory is possible. This checks things like the length of the filename and
// the link count of this directory. Returns EOK if adding the entry is possible.
// The expectation is that 'self' is locked before this function is called and
// that 'self' remains locked until after the directory entry has been added to
// self.
errno_t SfsDirectory_CanAcceptEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull name, FileType type)
{
    if (name->count > kSFSMaxFilenameLength) {
        return ENAMETOOLONG;
    }

    if (type == kFileType_Directory) {
        // Adding a subdirectory increments our link count by 1
        if (Inode_GetLinkCount(self) >= kSFSLimit_LinkMax) {
            return EMLINK;
        }
    }

    return EOK;
}

// Inserts a new directory entry of the form (pName, id) into the directory node
// 'self'. 'ih' is an optional insertion hint. If this pointer exists then the
// directory entry that it points to will be reused for the new directory entry;
// otherwise a completely new entry will be added to the directory.
// NOTE: this function does not verify that the new entry is unique. The caller
// has to ensure that it doesn't try to add a duplicate entry to the directory.
// NOTE: expects that you called SfsDirectory_CanAcceptEntry() before calling
// this function and that it returned EOK.
errno_t SfsDirectory_InsertEntry(InodeRef _Nonnull _Locked self, const PathComponent* _Nonnull name, InodeRef _Nonnull _Locked pChildNode, const sfs_insertion_hint_t* _Nullable ih)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    DiskBlockRef pBlock;
    ssize_t blockOffset;

    if (ih && ih->lba > 0) {
        try(FSContainer_AcquireBlock(fsContainer, ih->lba, kAcquireBlock_Update, &pBlock));
        blockOffset = ih->blockOffset;
    }
    else {
        sfs_bno_t fba;

        SfsFile_ConvertOffset((SfsFileRef)self, Inode_GetFileSize(self), &fba, &blockOffset);
        try(SfsFile_AcquireBlock((SfsFileRef)self, fba, kAcquireBlock_Update, &pBlock));
        try(SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer));
        Inode_IncrementFileSize(self, sizeof(sfs_dirent_t));
    }

    uint8_t* bp = DiskBlock_GetMutableData(pBlock);
    sfs_dirent_t* dep = (sfs_dirent_t*)(bp + blockOffset);

    memset(dep->filename, 0, kSFSMaxFilenameLength);
    memcpy(dep->filename, name->name, name->count);
    dep->len = name->count;
    dep->id = UInt32_HostToBig(Inode_GetId(pChildNode));

    FSContainer_RelinquishBlockWriting(fsContainer, pBlock, kWriteBlock_Sync);


    // Increment the link count of the directory if the child node is itself a
    // directory (accounting for its '..' entry)
    if (Inode_IsDirectory(pChildNode)) {
        Inode_Link(self);
    }


    // Mark the directory as modified
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);

catch:
    return err;
}


class_func_defs(SfsDirectory, SfsFile,
override_func_def(read, SfsDirectory, Inode)
);
