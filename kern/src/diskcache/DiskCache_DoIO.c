//
//  DiskCache_DoIO.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"
#include <assert.h>
#include <ext/nanotime.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>


static void _on_disk_op_done(DiskCacheRef _Nullable self, DiskOp* _Nullable op, errno_t err, ssize_t rlen);


// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1


errno_t DiskOp_Create(blkcnt_t clusterSize, DiskCacheRef _Nonnull cache, DiskOp* _Nullable * _Nonnull pOutOp)
{
    decl_try_err();
    const size_t iov_size = sizeof(struct iovec) * clusterSize;
    const size_t blk_size = sizeof(DiskBlockRef) * (clusterSize - 1);
    const size_t op_size = sizeof(struct DiskOp) + blk_size + iov_size;
    DiskOp* p = NULL;

    err = kalloc(op_size, (void*)&p);
    if (err == EOK) {
        p->qe = QUEUE_NODE_INIT;
        p->completion.f = (IOCompletionFunc)_on_disk_op_done;
        p->completion.ctx = cache;
        p->completion.arg = p;
        p->cnt = 0;
        p->iov = (iovec_t*)(((char*)p) + sizeof(DiskOp) + blk_size);
    }

    *pOutOp = p;
    return err;
}

void DiskOp_Destroy(DiskOp* _Nullable op)
{
    kfree(op);
}


// Blocks the caller until the given block has finished the given I/O operation
// type. Expects to be called with the lock held.
static errno_t _DiskCache_WaitIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, DiskBlockOp op)
{
    decl_try_err();

    while (pBlock->flags.op == op) {
        err = cnd_wait(&self->condition, &self->interlock);
        if (err != EOK) {
            return err;
        }
    }

    return EOK;
}

static DiskOp* _Nonnull _DiskCache_AcquireDiskOp(DiskCacheRef _Nonnull _Locked self, DiskSession* _Nonnull s)
{
    while (queue_empty(&s->dopsCache)) {
        s->wantsDop = true;
        assert(cnd_wait(&self->condition, &self->interlock) == EOK);
    }
    s->wantsDop = false;

    DiskOp* dop = (DiskOp*)queue_remove_first(&s->dopsCache);
    dop->session = s;
    queue_add_first(&s->dopsInUse, &dop->qe);
    
    return dop;
}

static void _DiskCache_RelinquishDiskOp(DiskCacheRef _Nonnull _Locked self, DiskOp* _Nonnull dop)
{
    DiskSession* s = dop->session;

    queue_remove_first(&s->dopsInUse);
    queue_add_first(&s->dopsCache, &dop->qe);

    if (s->wantsDop) {
        cnd_broadcast(&self->condition);
    }
}

static errno_t _DiskCache_StartReadOp(DiskCacheRef _Nonnull _Locked self, DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, bool isSync)
{
    decl_try_err();
    DiskOp* p = _DiskCache_AcquireDiskOp(self, s);

    // This is experimental: read all sectors in a single R/W cluster in one
    // go. This allows us to cache everything from a track right away. This makes
    // sense for track orientated disk drives like the Amiga disk drive. 
    const blkcnt_t nBlocksToCluster = s->rwClusterSize;
    const blkno_t lbaClusterStart = (nBlocksToCluster > 1) ? (pBlock->lba * s->s2bFactor) / s->rwClusterSize * s->rwClusterSize / s->s2bFactor : pBlock->lba;
    const off_t offset = lbaClusterStart * s->s2bFactor * s->sectorSize;
    size_t idx = 0;

    p->type = kIODiskCommand_Read;
    p->cnt = nBlocksToCluster;

    for (blkcnt_t i = 0; i < nBlocksToCluster; i++) {
        const blkno_t lba = lbaClusterStart + i;
        DiskBlockRef pOther;

        if (lba == pBlock->lba) {
            pBlock->flags.op = kDiskBlockOp_Read;
            pBlock->flags.async = (isSync) ? 0 : 1;
            pBlock->flags.readError = EOK;
        
            p->iov[idx].iov_base = pBlock->data;
            p->iov[idx].iov_len = self->blockSize - s->trailPadSize;
            p->blk[idx] = pBlock;
            idx++;
        }
        else {
            // We'll request all blocks in the request range that haven't already
            // been read in earlier. Note that we just ignore blocks that don't
            // fit our requirements since this is just for prefetching.
            err = _DiskCache_GetBlock(self, s, lba, kGetBlock_Allocate | kGetBlock_Exclusive, &pOther);
            if (err == EOK && !pOther->flags.hasData && pOther->flags.op != kDiskBlockOp_Read) {
                err = _DiskCache_LockBlockContent(self, pOther, kLockMode_Exclusive);

                if (err == EOK) {
                    // Trigger the async read. Note that endIO() will unlock-and-put the
                    // block for us.
                    ASSERT_LOCKED_EXCLUSIVE(pOther);
                    pOther->flags.op = kDiskBlockOp_Read;
                    pOther->flags.async = 1;
                    pOther->flags.readError = EOK;

                    p->iov[idx].iov_base = pOther->data;
                    p->iov[idx].iov_len = self->blockSize - s->trailPadSize;
                    p->blk[idx] = pOther;
                    idx++;
                }
            }
            if (err != EOK) {
                _DiskCache_PutBlock(self, pOther);
            }
        }
    }

    return DiskDriver_ReadAsync(s->disk, &p->iov[0], p->cnt, offset, &p->completion);
}

static errno_t _DiskCache_StartWriteOp(DiskCacheRef _Nonnull _Locked self, DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, bool isSync)
{
    DiskOp* p = _DiskCache_AcquireDiskOp(self, s);
    const off_t offset = pBlock->lba * s->s2bFactor * s->sectorSize;

    p->type = kIODiskCommand_Write;
    p->cnt = 1;
    p->iov[0].iov_base = pBlock->data;
    p->iov[0].iov_len = self->blockSize - s->trailPadSize;
    p->blk[0] = pBlock;

    pBlock->flags.op = kDiskBlockOp_Write;
    pBlock->flags.async = (isSync) ? 0 : 1;
    pBlock->flags.readError = EOK;

    return DiskDriver_WriteAsync(s->disk, p->iov, p->cnt, offset, &p->completion);
}

// Starts an operation to read the contents of the provided block from disk or
// to write it to disk. Must be called with the block locked in exclusive mode.
// Waits until the I/O operation is finished if 'isSync' is true. A synchronous
// I/O operation returns with the block locked in exclusive mode. If 'isSync' is
// false then the I/O operation is executed asynchronously and the block is
// unlocked and put once the I/O operation is done.
//
// NOTE: this function assumes that the data of a block that should be read from
// disk is zeroed out.
errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync)
{
    decl_try_err();
    IORWCommand* req = NULL;

    // Assert that if there is a I/O operation currently ongoing, that it is of
    // the same kind as 'op'. See the requirements at the top of this file.
    assert(pBlock->flags.op == kDiskBlockOp_Idle || pBlock->flags.op == op);


    // Reject a write if the block is pinned
    if (op == kDiskBlockOp_Write && pBlock->flags.isPinned) {
        return ENXIO;
    }


    // Just join an already ongoing I/O operation if one is active (and of the
    // same type as 'op')
    if (pBlock->flags.op == op) {
        return (isSync) ? _DiskCache_WaitIO(self, pBlock, op) : EOK;
    }


    // Start a new disk request
    switch (op) {
        case kDiskBlockOp_Read:
            err = _DiskCache_StartReadOp(self, s, pBlock, isSync);
            break;

        case kDiskBlockOp_Write:
            err = _DiskCache_StartWriteOp(self, s, pBlock, isSync);
            break;

        default:
            abort();
    }

    if (err == EOK && isSync) {
        err = _DiskCache_WaitIO(self, pBlock, op);
        // The lock is now held in exclusive mode again, if succeeded
    }

    return err;
}


// Must be called by the disk driver when it's done with a block.
// Expects that the block lock is held:
// - read: exclusively
// - write: shared
// If the operation is:
// - async: unlocks and puts the block
// - sync: wakes up the clients that are waiting on the block and leaves the block
//         locked exclusively
static void _DiskCache_OnBlockRequestDone(DiskCacheRef _Locked _Nonnull self, DiskBlockRef _Nonnull pBlock, int type, errno_t status)
{
    const bool isAsync = pBlock->flags.async ? true : false;

    switch (type) {
        case kIODiskCommand_Read:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            // Holding the exclusive lock here
            if (status == EOK) {
                pBlock->flags.hasData = 1;
            }
            break;

        case kIODiskCommand_Write:
            ASSERT_LOCKED_SHARED(pBlock);
            if (status == EOK && pBlock->flags.isDirty) {
                pBlock->flags.isDirty = 0;
                self->dirtyBlockCount--;
            }
            break;

        default:
            abort();
            break;
    }

    if (type == kIODiskCommand_Read) {
        // We only note read related errors since there's noone who could ever
        // look at a write-related error (because writes are often deferred and
        // thus they may happen a long time after the process that initiated the
        // write exited).
        pBlock->flags.readError = status;
    }
    pBlock->flags.async = 0;
    pBlock->flags.op = kDiskBlockOp_Idle;

    if (isAsync) {
        // Drops exclusive lock if this is a read op
        // Drops shared lock if this is a write op
        _DiskCache_UnlockContentAndPutBlock(self, pBlock);
        // Unlocked here 
    }
    else {
        // Wake up WaitIO()
        cnd_broadcast(&self->condition);
        // Will return with the lock held in exclusive or shared mode depending
        // on the type of I/O operation we did
    }
}

static void _on_disk_op_done(DiskCacheRef _Nullable self, DiskOp* _Nullable dop, errno_t err, ssize_t rlen)
{
    mtx_lock(&self->interlock);
    for (int i = 0; i < dop->cnt; i++) {
        DiskBlockRef pBlock = dop->blk[i];

        if (rlen >= self->blockSize) {
            rlen -= self->blockSize;
        }
        else if (err == EOK) {
            // We got a short read/write (bytes processed < block size) -> treat
            // this as an I/O error for now. XXX should probably retry the blocks
            // that got read/written short to get the real error from the disk
            // driver.
            err = EIO;
        }

        _DiskCache_OnBlockRequestDone(self, pBlock, dop->type, err);
    }

    _DiskCache_RelinquishDiskOp(self, dop);
    mtx_unlock(&self->interlock);
}
