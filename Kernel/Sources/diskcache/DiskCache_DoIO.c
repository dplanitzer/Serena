//
//  DiskCache_DoIO.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"

// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1


// Blocks the caller until the given block has finished the given I/O operation
// type. Expects to be called with the lock held.
static errno_t _DiskCache_WaitIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, DiskBlockOp op)
{
    decl_try_err();

    while (pBlock->flags.op == op) {
        err = ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity);
        if (err != EOK) {
            return err;
        }
    }

    return EOK;
}

// Starts an operation to read the contents of the provided block from disk or
// to write it to disk. Must be called with the block locked in exclusive mode.
// Waits until the I/O operation is finished if 'isSync' is true. A synchronous
// I/O operation returns with the block locked in exclusive mode. If 'isSync' is
// false then the I/O operation is executed asynchronously and the block is
// unlocked and put once the I/O operation is done.
errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync)
{
    decl_try_err();
    DiskRequest* req = NULL;

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


    //XXX
    // This is experimental: read/write all sectors in a single R/W cluster in one
    // go. This allows us to cache everything from a track right away. This makes
    // sense for track orientated disk drives like the Amiga disk drive. 
    const bcnt_t nBlocksToCluster = s->rwClusterSize;
    const bno_t lbaClusterStart = (nBlocksToCluster > 1) ? (pBlock->lba * s->s2bFactor) / s->rwClusterSize * s->rwClusterSize / s->s2bFactor : pBlock->lba;
    //XXX


    // Start a new disk request
    const int type = (op == kDiskBlockOp_Read) ? kDiskRequest_Read : kDiskRequest_Write;
    const size_t reqSize = sizeof(DiskRequest) + sizeof(IOVector) * (nBlocksToCluster - 1);
    size_t idx = 0;

    err = IORequest_Get(type, reqSize, (IORequest**)&req);
    if (err != EOK) {
        return err;
    }
    
#if 1
    for (bcnt_t i = 0; i < nBlocksToCluster; i++) {
        const LogicalBlockAddress lba = lbaClusterStart + i;
        DiskBlockRef pOther;

        if (lba == pBlock->lba) {
            pBlock->flags.op = op;
            pBlock->flags.async = (isSync) ? 0 : 1;
            pBlock->flags.readError = EOK;
        
            req->iov[idx].data = pBlock->data;
            req->iov[idx].token = (intptr_t)pBlock;
            req->iov[idx].size = self->blockSize - s->trailPadSize;
            idx++;
        }
        else if (op == kDiskBlockOp_Read) {
            // We'll request all blocks in the request range that haven't already
            // been read in earlier. Note that we just ignore blocks that don't
            // fit our requirements since this is just for prefetching.
            err = _DiskCache_GetBlock(self, s->disk, pBlock->mediaId, lba, kGetBlock_Allocate | kGetBlock_Exclusive, &pOther);
            if (err == EOK && !pOther->flags.hasData && pOther->flags.op != kDiskBlockOp_Read) {
                err = _DiskCache_LockBlockContent(self, pOther, kLockMode_Exclusive);

                if (err == EOK) {
                    // Trigger the async read. Note that endIO() will unlock-and-put the
                    // block for us.
                    ASSERT_LOCKED_EXCLUSIVE(pOther);
                    pOther->flags.op = op;
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
#else
    pBlock->flags.op = op;
    pBlock->flags.async = (isSync) ? 0 : 1;
    pBlock->flags.readError = EOK;

    req->iov[idx].lba = pBlock->lba;
    req->iov[idx].data = pBlock->data;
    req->iov[idx].token = (intptr_t)pBlock;
    idx++;
#endif

    req->s.done = (IODoneFunc)DiskCache_OnDiskRequestDone;
    req->s.context = self;
    req->refCon = (intptr_t)s->trailPadSize;
    req->mediaId = pBlock->mediaId;
    req->offset = lbaClusterStart * s->s2bFactor * s->sectorSize;
    req->iovCount = nBlocksToCluster;


    err = DiskDriver_BeginIO(s->disk, (IORequest*)req);
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
static void DiskCache_OnBlockRequestDone(DiskCacheRef _Nonnull self, DiskRequest* _Nonnull req, IOVector* _Nullable iov, errno_t status)
{
    Lock_Lock(&self->interlock);

    DiskBlockRef pBlock = (DiskBlockRef)iov->token;
    const size_t trailPadSize = (size_t)req->refCon;
    const bool isAsync = pBlock->flags.async ? true : false;

    switch (req->s.type) {
        case kDiskRequest_Read:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            // Holding the exclusive lock here
            if (status == EOK) {
                pBlock->flags.hasData = 1;

                if (trailPadSize > 0) {
                    // Sector size isn't a power-of-2 and this is a read. We zero-fill
                    // the remaining bytes in the cache block
                    memset(pBlock + self->blockSize - trailPadSize, 0, trailPadSize);
                }
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

    if (req->s.type == kDiskRequest_Read) {
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
        ConditionVariable_Broadcast(&self->condition);
        // Will return with the lock held in exclusive or shared mode depending
        // on the type of I/O operation we did
    }

    Lock_Unlock(&self->interlock);
}

void DiskCache_OnDiskRequestDone(DiskCacheRef _Nonnull self, DiskRequest* _Nonnull req, IOVector* _Nullable iov, errno_t status)
{
    if (iov) {
        DiskCache_OnBlockRequestDone(self, req, iov, status);
    }
    else {
        IORequest_Put((IORequest*)req);
    }
}
