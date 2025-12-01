//
//  DiskCache_DoIO.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"
#include <kern/timespec.h>

static void _on_disk_request_done(StrategyRequest* _Nonnull req);


// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1


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

static errno_t _DiskCache_CreateReadRequest(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, bool isSync, StrategyRequest* _Nullable * _Nonnull pOutReq)
{
    decl_try_err();
    StrategyRequest* req = NULL;

    // This is experimental: read all sectors in a single R/W cluster in one
    // go. This allows us to cache everything from a track right away. This makes
    // sense for track orientated disk drives like the Amiga disk drive. 
    const blkcnt_t nBlocksToCluster = s->rwClusterSize;
    const blkno_t lbaClusterStart = (nBlocksToCluster > 1) ? (pBlock->lba * s->s2bFactor) / s->rwClusterSize * s->rwClusterSize / s->s2bFactor : pBlock->lba;
    const size_t reqSize = sizeof(StrategyRequest) + sizeof(IOVector) * (nBlocksToCluster - 1);
    size_t idx = 0;

    err = IORequest_Get(kDiskRequest_Read, reqSize, (IORequest**)&req);
    if (err != EOK) {
        return err;
    }
    
    for (blkcnt_t i = 0; i < nBlocksToCluster; i++) {
        const blkno_t lba = lbaClusterStart + i;
        DiskBlockRef pOther;

        if (lba == pBlock->lba) {
            pBlock->flags.op = kDiskBlockOp_Read;
            pBlock->flags.async = (isSync) ? 0 : 1;
            pBlock->flags.readError = EOK;
        
            req->iov[idx].data = pBlock->data;
            req->iov[idx].token = (intptr_t)pBlock;
            req->iov[idx].size = self->blockSize - s->trailPadSize;
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

                    req->iov[idx].data = pOther->data;
                    req->iov[idx].token = (intptr_t)pOther;
                    req->iov[idx].size = self->blockSize - s->trailPadSize;
                    idx++;
                }
            }
            if (err != EOK) {
                _DiskCache_PutBlock(self, pOther);
            }
        }
    }

    req->s.item.retireFunc = (kdispatch_retire_func_t)_on_disk_request_done;
    req->offset = lbaClusterStart * s->s2bFactor * s->sectorSize;
    req->options = 0;
    req->iovCount = nBlocksToCluster;

    *pOutReq = req;
    return err;
}

static errno_t _DiskCache_CreateWriteRequest(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, bool isSync, StrategyRequest* _Nullable * _Nonnull pOutReq)
{
    decl_try_err();
    StrategyRequest* req = NULL;

    err = IORequest_Get(kDiskRequest_Write, sizeof(StrategyRequest), (IORequest**)&req);
    if (err != EOK) {
        return err;
    }
    
    req->s.item.retireFunc = (kdispatch_retire_func_t)_on_disk_request_done;
    req->offset = pBlock->lba * s->s2bFactor * s->sectorSize;
    req->options = 0;
    req->iovCount = 1;
    req->iov[0].data = pBlock->data;
    req->iov[0].token = (intptr_t)pBlock;
    req->iov[0].size = self->blockSize - s->trailPadSize;

    pBlock->flags.op = kDiskBlockOp_Write;
    pBlock->flags.async = (isSync) ? 0 : 1;
    pBlock->flags.readError = EOK;

    *pOutReq = req;
    return err;
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
errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync)
{
    decl_try_err();
    StrategyRequest* req = NULL;

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
            err = _DiskCache_CreateReadRequest(self, s, pBlock, isSync, &req);
            break;

        case kDiskBlockOp_Write:
            err = _DiskCache_CreateWriteRequest(self, s, pBlock, isSync, &req);
            break;

        default:
            abort();
    }

    if (err == EOK) {
       err = DiskDriver_BeginIO(s->disk, (IORequest*)req);
        if (err == EOK && isSync) {
            err = _DiskCache_WaitIO(self, pBlock, op);
            // The lock is now held in exclusive mode again, if succeeded
        }
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
        case kDiskRequest_Read:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            // Holding the exclusive lock here
            if (status == EOK) {
                pBlock->flags.hasData = 1;
            }
            break;

        case kDiskRequest_Write:
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

    if (type == kDiskRequest_Read) {
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

static void DiskCache_OnDiskRequestDone(DiskCacheRef _Nonnull self, StrategyRequest* _Nonnull req)
{
    ssize_t resCount = req->resCount;
    errno_t status = req->s.status;
    const int type = req->s.type;

    mtx_lock(&self->interlock);
    for (size_t i = 0; i < req->iovCount; i++) {
        DiskBlockRef pBlock = (DiskBlockRef)req->iov[i].token;

        if (resCount >= self->blockSize) {
            resCount -= self->blockSize;
        }
        else if (status == EOK) {
            // We got a short read/write (bytes processed < block size) -> treat
            // this as an I/O error for now. XXX should probably retry the blocks
            // that got read/written short to get the real error from the disk
            // driver.
            status = EIO;
        }

        _DiskCache_OnBlockRequestDone(self, pBlock, type, status);
    }
    mtx_unlock(&self->interlock);
}

static void _on_disk_request_done(StrategyRequest* _Nonnull req)
{
    DiskCache_OnDiskRequestDone(gDiskCache, req);
    IORequest_Put((IORequest*)req);
}
