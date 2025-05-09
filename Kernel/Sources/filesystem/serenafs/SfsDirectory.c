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
    off_t offset = IOChannel_GetOffset(ch);
    dirent_t* dp = buf;
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
    while (nSrcBytesToRead > 0 && nDstBytesToRead >= sizeof(dirent_t)) {
        const ssize_t nRemainderBlockSize = fs->blockAllocator.blockSize - blockOffset;
        ssize_t nBytesToReadInBlock = (nSrcBytesToRead > nRemainderBlockSize) ? nRemainderBlockSize : nSrcBytesToRead;
        SfsFileBlock blk;

        const errno_t e1 = SfsFile_MapBlock((SfsFileRef)self, blockIdx, kMapBlock_ReadOnly, &blk);
        if (e1 != EOK) {
            err = (nSrcBytesRead == 0) ? e1 : EOK;
            break;
        }
        
        const sfs_dirent_t* sp = (const sfs_dirent_t*)(blk.b.data + blockOffset);
        while (nBytesToReadInBlock > 0 && nDstBytesToRead >= sizeof(dirent_t)) {
            if (sp->id > 0) {
                dp->inid = UInt32_BigToHost(sp->id);
                memcpy(dp->name, sp->filename, sp->len);
                dp->name[sp->len] = '\0';
                
                nDstBytesRead += sizeof(dirent_t);
                nDstBytesToRead -= sizeof(dirent_t);
                dp++;
            }
            
            nBytesToReadInBlock -= sizeof(sfs_dirent_t);
            nSrcBytesToRead -= sizeof(sfs_dirent_t);
            nSrcBytesRead += sizeof(sfs_dirent_t);
            sp++;
        }
        SfsFile_UnmapBlock((SfsFileRef)self, &blk, kWriteBlock_None);

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

// Returns true if the function can establish that 'pDir' is a subdirectory of
// 'pAncestorDir' or that it is in fact 'pAncestorDir' itself.
bool SfsDirectory_IsAncestorOf(InodeRef _Nonnull _Locked pAncestorDir, InodeRef _Nonnull _Locked pGrandAncestorDir, InodeRef _Nonnull _Locked pDir)
{
    SerenaFSRef fs = Inode_GetFilesystemAs(pAncestorDir, SerenaFS);
    InodeRef pCurDir = Inode_Reacquire(pDir);
    bool r = false;

    while (true) {
        InodeRef pParentDir = NULL;
        bool didLock = false;

        if (Inode_Equals(pCurDir, pAncestorDir)) {
            r = true;
            break;
        }

        if (pCurDir != pDir && pCurDir != pAncestorDir && pCurDir != pGrandAncestorDir) {
            Inode_Lock(pCurDir);
            didLock = true;
        }
        const errno_t err = Filesystem_AcquireParentNode(fs, pCurDir, &pParentDir);
        if (didLock) {
            Inode_Unlock(pCurDir);
        }

        if (err != EOK || Inode_Equals(pCurDir, pParentDir)) {
            // Hit the root directory or encountered an error
            Inode_Relinquish(pParentDir);
            break;
        }

        Inode_Relinquish(pCurDir);
        pCurDir = pParentDir;
    }

catch:
    Inode_Relinquish(pCurDir);
    return r;
}

errno_t SfsDirectory_Query(InodeRef _Nonnull _Locked self, sfs_query_t* _Nonnull q, sfs_query_result_t* _Nonnull qr)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
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
        SfsFileBlock blk;
        
        err = SfsFile_MapBlock((SfsFileRef)self, blockIdx, kMapBlock_ReadOnly, &blk);
        if (err != EOK) {
            break;
        }
        
        const uint8_t* bp = blk.b.data;
        const sfs_dirent_t* sp = (const sfs_dirent_t*)bp;
        const sfs_dirent_t* ep = (const sfs_dirent_t*)(bp + fs->blockSize);

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
                qr->ih->lba = blk.lba;
                qr->ih->blockOffset = (const uint8_t*)sp - bp;
                hasInsertionHint = true;
            }
            
            if (done) {
                qr->id = UInt32_BigToHost(sp->id);
                if (qr->mpc) {
                    MutablePathComponent_SetString(qr->mpc, sp->filename, sp->len);
                }
                qr->lba = blk.lba;
                qr->blockOffset = (const uint8_t*)sp - bp;
                qr->fileOffset = offset;
                break;
            }

            offset += sizeof(sfs_dirent_t);
            sp++;
        }
        SfsFile_UnmapBlock((SfsFileRef)self, &blk, kWriteBlock_None);

        blockIdx++;
    }

    return (done) ? err : ENOENT;
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

    if (type == S_IFDIR) {
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
    ssize_t blockOffset;
    SfsFileBlock blk;

    if (ih && ih->lba > 0) {
        FSBlock fsblk;

        try(FSContainer_MapBlock(fsContainer, ih->lba, kMapBlock_Update, &fsblk));
        blk.b.token = fsblk.token;
        blk.b.data = fsblk.data;
        blk.lba = ih->lba;
        blk.wasAlloced = false;
        blockOffset = ih->blockOffset;
    }
    else {
        sfs_bno_t fba;

        SfsFile_ConvertOffset((SfsFileRef)self, Inode_GetFileSize(self), &fba, &blockOffset);
        try(SfsFile_MapBlock((SfsFileRef)self, fba, kMapBlock_Update, &blk));
        try(SfsAllocator_CommitToDisk(&fs->blockAllocator, fsContainer));
        Inode_IncrementFileSize(self, sizeof(sfs_dirent_t));
    }

    sfs_dirent_t* dep = (sfs_dirent_t*)(blk.b.data + blockOffset);

    memset(dep->filename, 0, kSFSMaxFilenameLength);
    memcpy(dep->filename, name->name, name->count);
    dep->len = name->count;
    dep->id = UInt32_HostToBig(Inode_GetId(pChildNode));

    SfsFile_UnmapBlock((SfsFileRef)self, &blk, kWriteBlock_Deferred);


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

errno_t SfsDirectory_RemoveEntry(InodeRef _Nonnull _Locked self, InodeRef _Nonnull _Locked pNodeToRemove)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    FSBlock blk = {0};
    sfs_query_t q;
    sfs_query_result_t qr;

    q.kind = kSFSQuery_InodeId;
    q.u.id = Inode_GetId(pNodeToRemove);
    q.mpc = NULL;
    q.ih = NULL;
    try(SfsDirectory_Query(self, &q, &qr));

    try(FSContainer_MapBlock(fsContainer, qr.lba, kMapBlock_Update, &blk));
    sfs_dirent_t* dep = (sfs_dirent_t*)(blk.data + qr.blockOffset);
    memset(dep, 0, sizeof(sfs_dirent_t));
    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);


    // Shrink the directory file by one entry if we removed the last entry in
    // the directory
    if (Inode_GetFileSize(self) - (off_t)sizeof(sfs_dirent_t) == qr.fileOffset) {
        SfsFile_Trim((SfsFileRef)self, qr.fileOffset);
    }


    // Reduce our link count by one if we removed a subdirectory
    if (Inode_IsDirectory(pNodeToRemove)) {
        Inode_Unlink(self);
    }


    // Mark the directory as modified
    Inode_SetModified(self, kInodeFlag_Updated | kInodeFlag_StatusChanged);

catch:
    return err;
}

errno_t SfsDirectory_UpdateParentEntry(InodeRef _Nonnull _Locked self, ino_t pnid)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);

    sfs_query_t q;
    sfs_query_result_t qr;
    FSBlock blk = {0};

    q.kind = kSFSQuery_PathComponent;
    q.u.pc = &kPathComponent_Parent;
    q.mpc = NULL;
    q.ih = NULL;
    try(SfsDirectory_Query(self, &q, &qr));

    try(FSContainer_MapBlock(fsContainer, qr.lba, kMapBlock_Update, &blk));

    sfs_dirent_t* dep = (sfs_dirent_t*)(blk.data + qr.blockOffset);
    dep->id = pnid;

    FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);

catch:
    return err;
}


class_func_defs(SfsDirectory, SfsFile,
override_func_def(read, SfsDirectory, Inode)
);
