//
//  SfsFile.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/17/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "SfsFile.h"
#include "SerenaFSPriv.h"
#include <assert.h>
#include <ext/endian.h>
#include <ext/timespec.h>
#include <filesystem/FSUtilities.h>


errno_t SfsFile_Create(Class* _Nonnull pClass, SerenaFSRef _Nonnull fs, ino_t inid, const sfs_inode_t* _Nonnull ip, InodeRef _Nullable * _Nonnull pOutNode)
{
    decl_try_err();
    SfsFileRef self;
    struct timespec at, mt, st;

    timespec_from(&at, UInt32_BigToHost(ip->accessTime.tv_sec), UInt32_BigToHost(ip->accessTime.tv_nsec));
    timespec_from(&mt, UInt32_BigToHost(ip->modificationTime.tv_sec), UInt32_BigToHost(ip->modificationTime.tv_nsec));
    timespec_from(&st, UInt32_BigToHost(ip->statusChangeTime.tv_sec), UInt32_BigToHost(ip->statusChangeTime.tv_nsec));

    err = Inode_Create(
        pClass,
        (FilesystemRef)fs,
        inid,
        UInt32_BigToHost(ip->mode),
        UInt32_BigToHost(ip->uid),
        UInt32_BigToHost(ip->gid),
        Int32_BigToHost(ip->linkCount),
        Int64_BigToHost(ip->size),
        &at,
        &mt,
        &st,
        UInt32_BigToHost(ip->pnid),
        (InodeRef*)&self);

    if (err == EOK) {
        self->bmap.indirect = ip->bmap.indirect;
        for (size_t i = 0; i < kSFSDirectBlockPointersCount; i++) {
            self->bmap.direct[i] = ip->bmap.direct[i];
        }
    }
    *pOutNode = (InodeRef)self;

    return err;
}

void SfsFile_Serialize(InodeRef _Nonnull _Locked pNode, sfs_inode_t* _Nonnull ip)
{
    SfsFileRef self = (SfsFileRef)pNode;
    struct timespec now;
    
    FSGetCurrentTime(&now);
    const struct timespec* atp = (Inode_IsAccessed(self)) ? &now : Inode_GetAccessTime(self);
    const struct timespec* mtp = (Inode_IsUpdated(self)) ? &now : Inode_GetModificationTime(self);
    const struct timespec* ctp = (Inode_IsStatusChanged(self)) ? &now : Inode_GetStatusChangeTime(self);

    ip->size = Int64_HostToBig(Inode_GetFileSize(self));
    ip->accessTime.tv_sec = UInt32_HostToBig(atp->tv_sec);
    ip->accessTime.tv_nsec = UInt32_HostToBig(atp->tv_nsec);
    ip->modificationTime.tv_sec = UInt32_HostToBig(mtp->tv_sec);
    ip->modificationTime.tv_nsec = UInt32_HostToBig(mtp->tv_nsec);
    ip->statusChangeTime.tv_sec = UInt32_HostToBig(ctp->tv_sec);
    ip->statusChangeTime.tv_nsec = UInt32_HostToBig(ctp->tv_nsec);
    ip->signature = UInt32_HostToBig(kSFSSignature_Inode);
    ip->id = UInt32_HostToBig(Inode_GetId(pNode));
    ip->pnid = UInt32_HostToBig(Inode_GetParentId(pNode));
    ip->linkCount = Int32_HostToBig(Inode_GetLinkCount(self));
    ip->uid = UInt32_HostToBig(Inode_GetUserId(self));
    ip->gid = UInt32_HostToBig(Inode_GetGroupId(self));
    ip->mode = UInt32_HostToBig(Inode_GetMode(self));

    ip->bmap.indirect = self->bmap.indirect;
    for (size_t i = 0; i < kSFSDirectBlockPointersCount; i++) {
        ip->bmap.direct[i] = self->bmap.direct[i];
    }
}

void SfsFile_ConvertOffset(SfsFileRef _Nonnull _Locked self, off_t offset, sfs_bno_t* _Nonnull pOutFba, ssize_t* _Nonnull pOutFbaOffset)
{
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);

    *pOutFba = (sfs_bno_t)(offset >> (off_t)fs->blockShift);
    *pOutFbaOffset = (ssize_t)(offset & (off_t)fs->blockMask);
}

// Maps the disk block 'lba' if lba is > 0; otherwise allocates a new block.
// The new block is for read-only if read-only 'mode' is requested and it is
// suitable for writing back to disk if 'mode' is a replace/update mode.
static errno_t map_disk_block(SerenaFSRef _Nonnull fs, blkno_t lba, MapBlock mode, sfs_bno_t* _Nonnull pOutOnDiskLba, SfsFileBlock* _Nonnull blk)
{
    decl_try_err();
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);

    blk->b.token = 0;
    blk->b.data = NULL;
    blk->lba = lba;
    blk->wasAlloced = false;
    blk->isZeroFill = false;

    if (lba > 0) {
        err = FSContainer_MapBlock(fsContainer, lba, mode, &blk->b);
    }
    else {
        if (mode == kMapBlock_ReadOnly) {
            blk->b.token = 0;
            blk->b.data = fs->emptyReadOnlyBlock;
            blk->isZeroFill = true;
            err = EOK;
        }
        else {
            blkno_t new_lba;

            if((err = SfsAllocator_Allocate(&fs->blockAllocator, &new_lba)) == EOK) {
                err = FSContainer_MapBlock(fsContainer, new_lba, kMapBlock_Cleared, &blk->b);
                
                if (err == EOK) {
                    blk->lba = new_lba;
                    blk->wasAlloced = true;
                    *pOutOnDiskLba = UInt32_HostToBig(new_lba);    
                }
                else {
                    SfsAllocator_Deallocate(&fs->blockAllocator, new_lba);
                }
            }
        }
    }

    return err;
}

// Maps the file block 'fba' in the file 'self'. Note that this function
// allocates a new file block if 'mode' implies a write operation and the required
// file block doesn't exist yet. However this function does not commit the updated
// allocation bitmap back to disk. The caller has to trigger this.
// This function expects that 'fba' is in the range 0..<numBlocksInFile.
errno_t SfsFile_MapBlock(SfsFileRef _Nonnull _Locked self, sfs_bno_t fba, MapBlock mode, SfsFileBlock* _Nonnull blk)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    sfs_bmap_t* bmap = &self->bmap;

    if (fba < kSFSDirectBlockPointersCount) {
        blkno_t dat_lba = UInt32_BigToHost(bmap->direct[fba]);

        return map_disk_block(fs, dat_lba, mode, &bmap->direct[fba], blk);
    }
    fba -= kSFSDirectBlockPointersCount;


    if (fba < fs->indirectBlockEntryCount) {
        SfsFileBlock i0_block;
        blkno_t i0_lba = UInt32_BigToHost(bmap->indirect);

        // Get the indirect block
        try(map_disk_block(fs, i0_lba, kMapBlock_Update, &bmap->indirect, &i0_block));


        // Get the data block
        sfs_bno_t* i0_bmap = (sfs_bno_t*)i0_block.b.data;
        blkno_t dat_lba = UInt32_BigToHost(i0_bmap[fba]);

        err = map_disk_block(fs, dat_lba, mode, &i0_bmap[fba], blk);
        
        FSContainer_UnmapBlock(fsContainer, i0_block.b.token, (blk->wasAlloced) ? kWriteBlock_Deferred : kWriteBlock_None);
        return err;
    }

    err = EFBIG;

catch:
    return err;
}

errno_t SfsFile_UnmapBlock(SfsFileRef _Nonnull _Locked self, SfsFileBlock* _Nonnull blk, WriteBlock mode)
{
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);

    if (mode == kWriteBlock_None && blk->isZeroFill) {
        return EOK;
    }
    if (mode != kWriteBlock_None && blk->isZeroFill) {
        // The empty block is for reading only
        abort();
    }

    return FSContainer_UnmapBlock(fsContainer, blk->b.token, mode);
}

// Trims (shortens) the size of the file to the new (and smaller) size 'newLength'.
// Note that this function may free blocks but it does not commit the changes to
// the allocation bitmap to the disk and doesn't set the inode modification flags.
// The caller has to do this. Note that this function always updates the file
// size even if no blocks are removed. It also always checks whether a block can
// be removed even if the file size of the node is already == newLength.
// Returns true if at least one block was actually trimmed; false otherwise
bool SfsFile_Trim(SfsFileRef _Nonnull _Locked self, off_t newLength)
{
    decl_try_err();
    SerenaFSRef fs = Inode_GetFilesystemAs(self, SerenaFS);
    FSContainerRef fsContainer = Filesystem_GetContainer(fs);
    const off_t oldLength = Inode_GetFileSize(self);
    sfs_bmap_t* bmap = &self->bmap;
    sfs_bno_t bn_nlen;
    ssize_t boff_nlen;
    bool didTrim = false;

    SfsFile_ConvertOffset(self, newLength, &bn_nlen, &boff_nlen);


    // Calculate the first FBA to discard
    sfs_bno_t bn_first_to_discard = (boff_nlen > 0) ? bn_nlen + 1 : bn_nlen;


    // Trim the direct blocks
    for (size_t bn = bn_first_to_discard; bn < kSFSDirectBlockPointersCount; bn++) {
        if (bmap->direct[bn] > 0) {
            SfsAllocator_Deallocate(&fs->blockAllocator, UInt32_BigToHost(bmap->direct[bn]));
            bmap->direct[bn] = 0;
            didTrim = true;
        }
    }


    // Figure out whether indirect blocks need to be trimmed
    const size_t bn_first_i0_to_discard = (bn_first_to_discard < kSFSDirectBlockPointersCount) ? 0 : bn_first_to_discard - kSFSDirectBlockPointersCount;
    const bool is_i0_update = (bn_first_i0_to_discard > 0) ? true : false;
    const blkno_t i0_lba = UInt32_BigToHost(bmap->indirect);
    FSBlock blk = {0};

    if (i0_lba > 0) {
        err = FSContainer_MapBlock(fsContainer, i0_lba, (is_i0_update) ? kMapBlock_Update : kMapBlock_ReadOnly, &blk);
        
        if (err == EOK) {
            sfs_bno_t* i0_bmap = (sfs_bno_t*)blk.data;

            for (size_t bn = bn_first_i0_to_discard; bn < fs->indirectBlockEntryCount; bn++) {
                if (i0_bmap[bn] > 0) {
                    SfsAllocator_Deallocate(&fs->blockAllocator, UInt32_BigToHost(i0_bmap[bn]));
                    if (is_i0_update) i0_bmap[bn] = 0;
                    didTrim = true;
                }
            }

            if (is_i0_update) {
                // We removed some of the i0 blocks
                FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_Deferred);
            }
            else {
                // We abandoned all of the i0 level
                FSContainer_UnmapBlock(fsContainer, blk.token, kWriteBlock_None);
                bmap->indirect = 0;
                didTrim = true;
            }
        }
    }

    Inode_SetFileSize(self, newLength);

    return didTrim;
}


class_func_defs(SfsFile, Inode,
);
