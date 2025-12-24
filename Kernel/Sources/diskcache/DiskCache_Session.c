//
//  DiskCache_Session.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"
#include <string.h>
#include <sched/delay.h>
#include <sched/vcpu.h>


// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1


void DiskCache_OpenSession(DiskCacheRef _Nonnull self, IOChannelRef _Nonnull chan, const disk_info_t* _Nonnull info, DiskSession* _Nonnull s)
{
    mtx_lock(&self->interlock);

    s->channel = IOChannel_Retain(chan);
    s->disk = IOChannel_GetResourceAs(chan, DiskDriver);
    s->sessionId = self->nextAvailSessionId;
    s->sectorSize = info->sectorSize;
    s->rwClusterSize = info->sectorsPerRdwr;
    s->activeMappingsCount = 0;
    s->isOpen = true;

    if (info->sectorSize > 0 && u_ispow2(info->sectorSize)) {
        s->s2bFactor = self->blockSize / info->sectorSize;
        s->trailPadSize = 0;
    }
    else {
        s->s2bFactor = 1;
        s->trailPadSize = self->blockSize - info->sectorSize;
    }

    self->nextAvailSessionId++;
    assert(self->nextAvailSessionId >= 0);  // no wrap around

    mtx_unlock(&self->interlock);
}

void DiskCache_CloseSession(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s)
{
    mtx_lock(&self->interlock);
    if (s->isOpen) {
        while (s->activeMappingsCount > 0) {
            // XXX would be nice to rely on the existing condition variable.
            // XXX however we don't want to have to introduce extra calls on it
            // XXX since unmap() calls happen a lot more than session closes. 
            mtx_unlock(&self->interlock);
            delay_ms(1);
            mtx_lock(&self->interlock);
        }

        IOChannel_Release(s->channel);
        s->channel = NULL;
        s->disk = NULL;
        s->sessionId = 0;
        s->isOpen = false;
    }
    mtx_unlock(&self->interlock);
}


// Triggers an asynchronous loading of the disk block data at the address
// (sessionId, lba)
static errno_t _DiskCache_PrefetchBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, blkno_t lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;
    bool doingIO = false;

    // Get the block
    err = _DiskCache_GetBlock(self, s, lba, kGetBlock_Allocate | kGetBlock_RecentUse | kGetBlock_Exclusive, &pBlock);
    if (err == EOK && !pBlock->flags.hasData && pBlock->flags.op != kDiskBlockOp_Read) {
        err = _DiskCache_LockBlockContent(self, pBlock, kLockMode_Exclusive);

        if (err == EOK) {
            // Trigger the async read. Note that endIO() will unlock-and-put the
            // block for us.
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            err = _DiskCache_DoIO(self, s, pBlock, kDiskBlockOp_Read, false);
            doingIO = (err == EOK) ? true : false;
        }
    }
    if (!doingIO && pBlock) {
        _DiskCache_UnlockContentAndPutBlock(self, pBlock);
    }

    return err;
}

// Triggers an asynchronous loading of the disk block data at the address
// (sessionId, lba)
errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba)
{
    decl_try_err();

    mtx_lock(&self->interlock);
    if (s->isOpen) {
        err = _DiskCache_PrefetchBlock(self, s, lba);
    }
    else {
        err = ENODEV;
    }
    mtx_unlock(&self->interlock);

    return err;
}


// Check whether the given block has dirty data and write it synchronously to
// disk, if so.
errno_t _DiskCache_SyncBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef pBlock)
{
    decl_try_err();

    if (pBlock->flags.isDirty && pBlock->flags.op != kDiskBlockOp_Write) {
        err = _DiskCache_LockBlockContent(self, pBlock, kLockMode_Shared);
        
        if (err == EOK) {
            ASSERT_LOCKED_SHARED(pBlock);
            err = _DiskCache_DoIO(self, s, pBlock, kDiskBlockOp_Write, true);
            _DiskCache_UnlockBlockContent(self, pBlock);
        }
    }

    return err;
}

errno_t DiskCache_SyncBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    mtx_lock(&self->interlock);

    if (s->isOpen) {
        // Find the block and only sync it if no one else is currently using it
        if ((err = _DiskCache_GetBlock(self, s, lba, kGetBlock_Exclusive, &pBlock)) == EOK) {
            err = _DiskCache_SyncBlock(self, s, pBlock);
            _DiskCache_PutBlock(self, pBlock);
        }
    }
    else {
        err = ENODEV;
    }

    mtx_unlock(&self->interlock);

    return err;
}

errno_t DiskCache_PinBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    mtx_lock(&self->interlock);

    if (s->isOpen) {
        if ((err = _DiskCache_GetBlock(self, s, lba, 0, &pBlock)) == EOK) {
            pBlock->flags.isPinned = 1;
            _DiskCache_PutBlock(self, pBlock);
        }
    }
    else {
        err = ENODEV;
    }

    mtx_unlock(&self->interlock);
    return err;
}

errno_t DiskCache_UnpinBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    mtx_lock(&self->interlock);

    if (s->isOpen) {
        if ((err = _DiskCache_GetBlock(self, s, lba, 0, &pBlock)) == EOK) {
            pBlock->flags.isPinned = 0;
            _DiskCache_PutBlock(self, pBlock);
        }
    }
    else {
        err = ENODEV;
    }

    mtx_unlock(&self->interlock);
    return err;
}

errno_t DiskCache_MapBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, blkno_t lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    blk->token = 0;
    blk->data = NULL;


    mtx_lock(&self->interlock);

    if (!s->isOpen) {
        throw(ENODEV);
    }


    // Get and lock the block. Only time we can lock the block content shared is
    // when the caller asks for read-only and the block content already exists.
    // Lock for exclusive mode in all other cases.
    err = _DiskCache_GetBlock(self, s, lba, kGetBlock_Allocate | kGetBlock_RecentUse, &pBlock);
    if (err != EOK) {
        mtx_unlock(&self->interlock);
        return err;
    }

    const LockMode lockMode = (mode == kMapBlock_ReadOnly && pBlock->flags.hasData) ? kLockMode_Shared : kLockMode_Exclusive;
    err = _DiskCache_LockBlockContent(self, pBlock, lockMode);
    if (err != EOK) {
        _DiskCache_PutBlock(self, pBlock);
        mtx_unlock(&self->interlock);
        return err;
    }


    switch (mode) {
        case kMapBlock_Cleared:
            // We always clear the block data because we don't know whether the
            // data is all zero or not
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            memset(pBlock->data, 0, self->blockSize);
            pBlock->flags.hasData = 1;
            break;

        case kMapBlock_Replace:
            // Caller accepts whatever is currently in the buffer since it's
            // going to replace every byte anyway.
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            pBlock->flags.hasData = 1;
            break;

        case kMapBlock_Update:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            if (!pBlock->flags.hasData) {
                err = _DiskCache_DoIO(self, s, pBlock, kDiskBlockOp_Read, true);
                if (err == EOK && pBlock->flags.readError != EOK) {
                    err = pBlock->flags.readError;
                }
            }
            break;

        case kMapBlock_ReadOnly:
            if (!pBlock->flags.hasData) {
                ASSERT_LOCKED_EXCLUSIVE(pBlock);

                err = _DiskCache_DoIO(self, s, pBlock, kDiskBlockOp_Read, true);
                if (err == EOK && pBlock->flags.readError != EOK) {
                    err = pBlock->flags.readError;
                }

                _DiskCache_DowngradeBlockContentLock(self, pBlock);
            }
            break;

        default:
            abort();
            break;
    }

catch:
    if (pBlock) {
        if (err == EOK) {
            blk->token = (intptr_t)pBlock;
            blk->data = pBlock->data;
            s->activeMappingsCount++;
        }
        else {
            _DiskCache_UnlockContentAndPutBlock(self, pBlock);
        }
    }

    mtx_unlock(&self->interlock);

    return err;
}


errno_t DiskCache_UnmapBlock(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s, intptr_t token, WriteBlock mode)
{
    decl_try_err();
    DiskBlockRef pBlock = (DiskBlockRef)token;

    if (pBlock == NULL) {
        return EOK;
    }
#if defined(__FORCE_WRITES_SYNC)
    if (mode == kWriteBlock_Deferred) {
        mode = kWriteBlock_Sync;
    }
#endif

    mtx_lock(&self->interlock);

    if (!s->isOpen) {
        throw(ENODEV);
    }

    switch (mode) {
        case kWriteBlock_None:
            break;

        case kWriteBlock_Sync:
            // We must be holding the exclusive lock here
            ASSERT_LOCKED_EXCLUSIVE(pBlock);

            _DiskCache_DowngradeBlockContentLock(self, pBlock);
            ASSERT_LOCKED_SHARED(pBlock);
            err = _DiskCache_DoIO(self, s, pBlock, kDiskBlockOp_Write, true);
            break;

        case kWriteBlock_Deferred:
            // We must be holding the exclusive lock here
            ASSERT_LOCKED_EXCLUSIVE(pBlock);

            if (pBlock->flags.isDirty == 0) {
                pBlock->flags.isDirty = 1;
                self->dirtyBlockCount++;
                // Block data will be written out when needed
            }
            break;

        default:
            abort();
            break;
    }
    s->activeMappingsCount--;

    _DiskCache_UnlockContentAndPutBlock(self, pBlock);

catch:
    mtx_unlock(&self->interlock);

    return err;
}

// Synchronously writes all dirty disk blocks belonging to the session 's' to
// disk.
errno_t DiskCache_Sync(DiskCacheRef _Nonnull self, DiskSession* _Nonnull s)
{
    decl_try_err();

    mtx_lock(&self->interlock);
    if (!s->isOpen) {
        throw(ENODEV);
    }

    if (self->dirtyBlockCount > 0) {
        size_t myLruChainGeneration = self->lruChainGeneration;
        bool done = false;

        // We push dirty blocks to the disk, starting at the block that has been
        // sitting dirty in memory for the longest time. Note that we drop the
        // interlock while writing the block to the disk. This is why we need to
        // check here whether the LRU chain has changed on us while we were unlocked.
        // If so, we restart the iteration.
        while (!done) {
            done = true;

            List_ForEachReversed(&self->lruChain, ListNode, 
                DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);

                if (!DiskBlock_InUse(pb) && (pb->flags.isDirty && !pb->flags.isPinned)) {
                    if (pb->sessionId == s->sessionId) {
                        const errno_t err1 = _DiskCache_SyncBlock(self, s, pb);
                    
                        if (err == EOK) {
                            // Return the first error that we encountered. However,
                            // we continue flushing as many blocks as we can
                            err = err1;
                        }
                    }
                    _DiskCache_PutBlock(self, pb);
                }

                if (myLruChainGeneration != self->lruChainGeneration) {
                    done = false;
                    break;
                }
            );
        }
    }

catch:
    mtx_unlock(&self->interlock);

    return err;
}
