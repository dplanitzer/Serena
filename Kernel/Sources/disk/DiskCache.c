//
//  DiskCache.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"
#include <hal/MonotonicClock.h>
#include <log/Log.h>

// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1


DiskCacheRef _Nonnull  gDiskCache;

errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskCache* self;
    
    assert(__ELAST <= UCHAR_MAX);
    
    try(kalloc_cleared(sizeof(DiskCache), (void**) &self));
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Background, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->autoSyncQueue));
    try(DiskBlock_Create(NULL, kMediaId_None, 0, &self->emptyBlock));

    Lock_Init(&self->interlock);
    ConditionVariable_Init(&self->condition);
    List_Init(&self->lruChain);

    self->blockCount = 0;
    self->blockCapacity = SystemDescription_GetRamSize(pSysDesc) >> 5;
    assert(self->blockCapacity > 0);

    for (int i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_Init(&self->diskAddrHash[i]);
    }

    _DiskCache_ScheduleAutoSync(self);

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

// Locks the given block's content in shared or exclusive mode. Multiple clients
// may lock the content of a block in shared mode but at most one client at a
// time may lock the content of a block in exclusive mode. A block is only
// lockable in exclusive mode if no other client is locking it in shared or
// exclusive mode at the same time.
static errno_t _DiskCache_LockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, LockMode mode)
{
    decl_try_err();

    for (;;) {
        switch (mode) {
            case kLockMode_Shared:
                if (pBlock->flags.exclusive == 0) {
                    pBlock->shareCount++;
                    return EOK;
                }
                break;

            case kLockMode_Exclusive:
                if ((pBlock->flags.exclusive == 0) && (pBlock->shareCount == 0)) {
                    pBlock->flags.exclusive = 1;
                    return EOK;
                }
                break;

            default:
                abort();
                break;
        }

        err = ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity);
        if (err != EOK) {
            break;
        }
    }

    return err;
}

// Unlock the given block's content. This function assumes that if the block
// content is currently locked exclusively, that the caller is indeed the owner
// of the block content since there can only be a single exclusive locker. If
// the block content is locked in shared mode instead, then the caller is
// assumed to be one of the shared block owners.
static void _DiskCache_UnlockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    if (pBlock->flags.exclusive) {
        // The lock is being held exclusively - we assume that we are holding it. Unlock it
        pBlock->flags.exclusive = 0;
    }
    else if (pBlock->shareCount > 0) {
        // The lock is being held in shared mode. Unlock it
        pBlock->shareCount--;
    }
    else {
        abort();
    }

    ConditionVariable_Broadcast(&self->condition);
}

#if 0
// Upgrades the given block content lock from being locked shared to locked
// exclusive. Expects that the caller is holding a shared lock on the block
static errno_t _DiskCache_UpgradeBlockContentLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    decl_try_err();

    assert(pBlock->shareCount > 0);

    while (pBlock->shareCount > 1 && pBlock->flags.exclusive) {
        err = ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity);
        if (err != EOK) {
            return err;
        }
    }

    pBlock->shareCount--;
    pBlock->flags.exclusive = 1;

    return EOK;
}
#endif

// Downgrades the given block content lock from exclusive mode to shared mode.
// Expects that the caller is holding an exclusive lock on the block.
static void _DiskCache_DowngradeBlockContentLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    ASSERT_LOCKED_EXCLUSIVE(pBlock);

    pBlock->flags.exclusive = 0;
    pBlock->shareCount++;

    // Purposefully do not give other clients who are waiting to be able to lock
    // this block exclusively a chance to do so. First, we want to do the
    // exclusive -> shared transition atomically and second none else would be
    // able to lock exclusively anyway since we now own the lock shared
}


#define DiskBlockFromLruChainPointer(__ptr) \
(DiskBlockRef) (((uint8_t*)__ptr) - offsetof(struct DiskBlock, lruNode))

static void _DiskCache_RegisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];

    List_InsertBeforeFirst(chain, &pBlock->hashNode);
    List_InsertBeforeFirst(&self->lruChain, &pBlock->lruNode);
    self->lruChainGeneration++;
}

static void _DiskCache_UnregisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];

    List_Remove(chain, &pBlock->hashNode);
    List_Remove(&self->lruChain, &pBlock->lruNode);
    self->lruChainGeneration++;
}
#if 0
static void _DiskCache_Print(DiskCacheRef _Nonnull _Locked self)
{
    printf("{");
    for (size_t i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->diskAddrHash[i], DiskBlock,
            printf("%u [%u], ", pCurNode->lba, i);
        );
    }
    printf("}");
}

static void _DiskCache_PrintLruChain(DiskCacheRef _Nonnull _Locked self)
{
    printf("{");
    List_ForEach(&self->lruChain, ListNode,
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);
        printf("%u", pb->lba);
        if (pCurNode->next) {
            printf(", ");
        }
    );
    printf("}");
}
#endif


static errno_t _DiskCache_CreateBlock(DiskCacheRef _Nonnull _Locked self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    // We can still grow the disk block list
    err = DiskBlock_Create(disk, mediaId, lba, &pBlock);
    if (err == EOK) {
        _DiskCache_RegisterBlock(self, pBlock);
        self->blockCount++;
    }
    *pOutBlock = pBlock;

    return err;
}

// Finds the oldest cached block that isn't currently in use and re-targets this
// block to the new disk address.  
static DiskBlockRef _DiskCache_ReuseCachedBlock(DiskCacheRef _Nonnull _Locked self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba)
{
    DiskBlockRef pBlock = NULL;

    List_ForEachReversed(&self->lruChain, ListNode, 
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);

        if (!DiskBlock_InUse(pb)) {
            pBlock = pb;
            break;
        }
    );

    if (pBlock) {
        // Sync the block to disk if necessary
        _DiskCache_SyncBlock(self, pBlock);

        _DiskCache_UnregisterBlock(self, pBlock);
        DiskBlock_SetDiskAddress(pBlock, disk, mediaId, lba);
        _DiskCache_RegisterBlock(self, pBlock);
    }

    return pBlock;
}

// Returns the block that corresponds to the disk address (disk, mediaId, lba).
// A new block is created if needed or an existing block is retrieved from the
// cached list of blocks. The caller must lock the content of the block before
// doing I/O on it or before handing it to a filesystem.
static errno_t _DiskCache_GetBlock(DiskCacheRef _Nonnull _Locked self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba, unsigned int options, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    for (;;) {
        // Look up the block based on (disk, mediaId, lba)
        const size_t idx = DiskBlock_HashKey(disk, mediaId, lba) & DISK_BLOCK_HASH_CHAIN_MASK;
        List* chain = &self->diskAddrHash[idx];
    
        pBlock = NULL;
        List_ForEach(chain, DiskBlock,
            if (DiskBlock_IsEqualKey(pCurNode, disk, mediaId, lba)) {
                pBlock = pCurNode;
                break;
            }
        );


        if (pBlock || (pBlock == NULL && (options & kGetBlock_Allocate) == 0)) {
            break;
        }


        // Either allocate a new block if the cache is still allowed to grow or
        // find a block that we can reuse for the new disk address
        if (self->blockCount < self->blockCapacity) {
            // We can still grow the disk block list
            err = _DiskCache_CreateBlock(self, disk, mediaId, lba, &pBlock);
            break;
        }
        else {
            // We can't create any more disk blocks. Try to reuse one that isn't
            // currently in use. We may have to wait for a disk block to become
            // available for use if they are all currently in use.
            pBlock = _DiskCache_ReuseCachedBlock(self, disk, mediaId, lba);
            if (pBlock) {
                break;
            }

            try_bang(ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity));
        }
    }


    if (pBlock) {
        if ((options & kGetBlock_Exclusive) == kGetBlock_Exclusive && DiskBlock_InUse(pBlock)) {
            pBlock = NULL;
        }

        if (pBlock) {
            if ((options & kGetBlock_RecentUse) == kGetBlock_RecentUse) {
                List_Remove(&self->lruChain, &pBlock->lruNode);
                List_InsertBeforeFirst(&self->lruChain, &pBlock->lruNode);
                self->lruChainGeneration++;
            }
        }
    }

    *pOutBlock = pBlock;
    return err;
}

static void _DiskCache_PutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    if (!DiskBlock_InUse(pBlock)) {
        assert(pBlock->flags.op == kDiskBlockOp_Idle);

        // Wake the wait() in _DiskBlock_Get() if this isn't the (singleton) empty block
        if (pBlock != self->emptyBlock) {
            ConditionVariable_Broadcast(&self->condition);
        }
    }
}

static void _DiskCache_UnlockContentAndPutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock)
{
    _DiskCache_UnlockBlockContent(self, pBlock);
    _DiskCache_PutBlock(self, pBlock);
}

// Purges all cached blocks of the disk drive 'disk' and media 'mediaId'. The
// media ID may be kMedia_None which means that all blocks of the driver 'disk'
// should be purged no matter for which media they hold data.
// Purging a block means that the block is reset back to a state where it is forced
// to read its data in again from the disk, next time data is requested.
static void _DiskCache_PurgeBlocks(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId)
{
    // XXX optimize this
    List_ForEach(&self->lruChain, DiskBlock, 
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);

        if (pb->disk == disk && (mediaId == kMediaId_None || mediaId == pb->mediaId)) {
            // XXX do something about blocks that are currently doing I/O (cancel I/O)
            assert(pb->flags.op == kDiskBlockOp_Idle);
            DiskBlock_Purge(pb);
        }
    );
}


//
// API
//

errno_t DiskCache_RegisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk)
{
    decl_try_err();

    Lock_Lock(&self->interlock);

    DiskCacheClient* dcc = DiskDriver_GetDiskCacheClient(disk);
    if (dcc->state == kDccS_NotRegistered) {
        dcc->useCount = 0;
        dcc->state = kDccS_Registered;
        Object_Retain(disk);
        err = EOK;
    }
    else {
        err = EBUSY;
    }

    Lock_Unlock(&self->interlock);

    return err;
}

static void _DiskCache_FinalizeUnregisterDisk(DiskCacheRef _Nonnull _Locked self, DiskDriverRef _Nonnull disk)
{
    DiskCacheClient* dcc = DiskDriver_GetDiskCacheClient(disk);

    dcc->state = kDccS_NotRegistered;
    dcc->useCount = 0;

    _DiskCache_PurgeBlocks(self, disk, kMediaId_None);
    Object_Release(disk);
}

void DiskCache_UnregisterDisk(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk)
{
    Lock_Lock(&self->interlock);

    DiskCacheClient* dcc = DiskDriver_GetDiskCacheClient(disk);
    if (dcc->state == kDccS_Registered) {
        dcc->state = kDccS_Deregistering;

        if (dcc->useCount == 0) {
            _DiskCache_FinalizeUnregisterDisk(self, disk);
        }
    }

    Lock_Unlock(&self->interlock);
}

// Triggers an asynchronous loading of the disk block data at the address
// (disk, mediaId, lba)
errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;
    bool doingIO = false;

    // Can not address blocks on a disk or media that doesn't exist
    if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Get the block
    err = _DiskCache_GetBlock(self, disk, mediaId, lba, kGetBlock_Allocate | kGetBlock_RecentUse | kGetBlock_Exclusive, &pBlock);
    if (err == EOK && !pBlock->flags.hasData && pBlock->flags.op != kDiskBlockOp_Read) {
        err = _DiskCache_LockBlockContent(self, pBlock, kLockMode_Exclusive);

        if (err == EOK) {
            // Trigger the async read. Note that endIO() will unlock-and-put the
            // block for us.
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, false);
            doingIO = (err == EOK) ? true : false;
        }
    }
    if (!doingIO && pBlock) {
        _DiskCache_UnlockContentAndPutBlock(self, pBlock);
    }

    Lock_Unlock(&self->interlock);

    return err;
}

// Check whether the given block has dirty data and write it synchronously to
// disk, if so.
static errno_t _DiskCache_SyncBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef pBlock)
{
    decl_try_err();

    if (pBlock->flags.isDirty && pBlock->flags.op != kDiskBlockOp_Write) {
        err = _DiskCache_LockBlockContent(self, pBlock, kLockMode_Shared);
        
        if (err == EOK) {
            ASSERT_LOCKED_SHARED(pBlock);
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
            _DiskCache_UnlockBlockContent(self, pBlock);
        }
    }

    return err;
}

errno_t DiskCache_SyncBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Can not address blocks on a disk or media that doesn't exist
    if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Find the block and only sync it if no one else is currently using it
    err = _DiskCache_GetBlock(self, disk, mediaId, lba, kGetBlock_Exclusive, &pBlock);
    if (err == EOK) {
        err = _DiskCache_SyncBlock(self, pBlock);
        _DiskCache_PutBlock(self, pBlock);
    }

    Lock_Unlock(&self->interlock);

    return err;
}

// Maps an empty read-only block (all data is zero).
errno_t DiskCache_MapEmptyBlock(DiskCacheRef _Nonnull self, FSBlock* _Nonnull blk)
{
    decl_try_err();

    Lock_Lock(&self->interlock);    
    err = _DiskCache_LockBlockContent(self, self->emptyBlock, kLockMode_Shared);
    if (err == EOK) {
        blk->token = (intptr_t)self->emptyBlock;
        blk->data = DiskBlock_GetMutableData(self->emptyBlock);
    }
    else {
        blk->token = 0;
        blk->data = NULL;
    }
    Lock_Unlock(&self->interlock);

    return err;
}

errno_t DiskCache_MapBlock(DiskCacheRef _Nonnull self, DiskDriverRef _Nonnull disk, MediaId mediaId, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    blk->token = 0;
    blk->data = NULL;


    // Can not address blocks on a disk or media that doesn't exist
    if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Get and lock the block. Only time we can lock the block content shared is
    // when the caller asks for read-only and the block content already exists.
    // Lock for exclusive mode in all other cases.
    err = _DiskCache_GetBlock(self, disk, mediaId, lba, kGetBlock_Allocate | kGetBlock_RecentUse, &pBlock);
    if (err != EOK) {
        Lock_Unlock(&self->interlock);
        return err;
    }

    const LockMode lockMode = (mode == kMapBlock_ReadOnly && pBlock->flags.hasData) ? kLockMode_Shared : kLockMode_Exclusive;
    err = _DiskCache_LockBlockContent(self, pBlock, lockMode);
    if (err != EOK) {
        _DiskCache_PutBlock(self, pBlock);
        Lock_Unlock(&self->interlock);
        return err;
    }


    switch (mode) {
        case kMapBlock_Cleared:
            // We always clear the block data because we don't know whether the
            // data is all zero or not
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            memset(pBlock->data, 0, pBlock->flags.byteSize);
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
                err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, true);
                if (err == EOK && pBlock->flags.readError != EOK) {
                    err = pBlock->flags.readError;
                }
            }
            break;

        case kMapBlock_ReadOnly:
            if (!pBlock->flags.hasData) {
                ASSERT_LOCKED_EXCLUSIVE(pBlock);

                err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, true);
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
            blk->data = DiskBlock_GetMutableData(pBlock);
        }
        else {
            _DiskCache_UnlockContentAndPutBlock(self, pBlock);
        }
    }

    Lock_Unlock(&self->interlock);

    return err;
}


void DiskCache_UnmapBlock(DiskCacheRef _Nonnull self, intptr_t token)
{
    DiskBlockRef pBlock = (DiskBlockRef)token;

    if (pBlock) {
        Lock_Lock(&self->interlock);
        _DiskCache_UnlockContentAndPutBlock(self, pBlock);
        Lock_Unlock(&self->interlock);
    }
}

errno_t DiskCache_UnmapBlockWriting(DiskCacheRef _Nonnull self, intptr_t token, WriteBlock mode)
{
    decl_try_err();
    DiskBlockRef pBlock = (DiskBlockRef)token;

    if (pBlock == NULL) {
        return EOK;
    }
    if (pBlock == self->emptyBlock) {
        // The empty block is for reading only
        abort();
    }
#if defined(__FORCE_WRITES_SYNC)
    mode = kWriteBlock_Sync;
#endif

    Lock_Lock(&self->interlock);

    // We must be holding the exclusive lock here
    ASSERT_LOCKED_EXCLUSIVE(pBlock);

    switch (mode) {
        case kWriteBlock_Sync:
            _DiskCache_DowngradeBlockContentLock(self, pBlock);
            ASSERT_LOCKED_SHARED(pBlock);
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
            break;

        case kWriteBlock_Deferred:
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

    _DiskCache_UnlockContentAndPutBlock(self, pBlock);

    Lock_Unlock(&self->interlock);

    return err;
}


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
static errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync)
{
    decl_try_err();

    // Assert that if there is a I/O operation currently ongoing, that it is of
    // the same kind as 'op'. See the requirements at the top of this file.
    assert(pBlock->flags.op == kDiskBlockOp_Idle || pBlock->flags.op == op);


    // Just join an already ongoing I/O operation if one is active (and of the
    // same type as 'op')
    if (pBlock->flags.op == op) {
        return (isSync) ? _DiskCache_WaitIO(self, pBlock, op) : EOK;
    }


    // Don't start a new I/O request if the driver has been deregistered
    DiskDriverRef disk = pBlock->disk;
    DiskCacheClient* dcc = DiskDriver_GetDiskCacheClient(disk);

    if (dcc->state != kDccS_Registered) {
        return ENODEV;
    }


    // Start a new I/O operation
    pBlock->flags.op = op;
    pBlock->flags.async = (isSync) ? 0 : 1;
    pBlock->flags.readError = EOK;

    IORequest ior;
    ior.block = pBlock;
    ior.mediaId = pBlock->mediaId;
    ior.lba = pBlock->lba;

    err = DiskDriver_BeginIO(disk, &ior);
    if (err == EOK && isSync) {
        dcc->useCount++;
        err = _DiskCache_WaitIO(self, pBlock, op);
        // The lock is now held in exclusive mode again, if succeeded
    }

    return err;
}


// Must be called by the disk driver when it's done with the block.
// Expects that the block lock is held:
// - read: exclusively
// - write: shared
// If the operation is:
// - async: unlocks and puts the block
// - sync: wakes up the clients that are waiting on the block and leaves the block
//         locked exclusively
void DiskCache_OnBlockFinishedIO(DiskCacheRef _Nonnull self, DiskBlockRef _Nonnull pBlock, errno_t status)
{
    Lock_Lock(&self->interlock);

    const bool isAsync = pBlock->flags.async ? true : false;

    switch (pBlock->flags.op) {
        case kDiskBlockOp_Read:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            // Holding the exclusive lock here
            if (status == EOK) {
                pBlock->flags.hasData = 1;
            }
            break;

        case kDiskBlockOp_Write:
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

    if (pBlock->flags.op == kDiskBlockOp_Read) {
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


    // Balance the useCount from DoIO()
    DiskDriverRef disk = pBlock->disk;
    DiskCacheClient* dcc = DiskDriver_GetDiskCacheClient(disk);
    dcc->useCount--;
    if (dcc->useCount == 0 && dcc->state == kDccS_Deregistering) {
        _DiskCache_FinalizeUnregisterDisk(self, disk);
    }

    Lock_Unlock(&self->interlock);
}


// Synchronously writes all dirty disk blocks for drive 'diskId' and media
// 'mediaId' to disk. Flushes all dirty blocks for all drives and media if
// 'disk' is NULL. 
errno_t DiskCache_Sync(DiskCacheRef _Nonnull self, DiskDriverRef _Nullable disk, MediaId mediaId)
{
    decl_try_err();

    if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }

    Lock_Lock(&self->interlock);
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
                DiskBlockRef pBlock = DiskBlockFromLruChainPointer(pCurNode);

                if (!DiskBlock_InUse(pBlock)) {
                    if ((disk == NULL) || (pBlock->disk == disk && (pBlock->mediaId == mediaId || mediaId == kMediaId_Current))) {
                        const errno_t err1 = _DiskCache_SyncBlock(self, pBlock);
                    
                        if (err == EOK) {
                            // Return the first error that we encountered. However,
                            // we continue flushing as many blocks as we can
                            err = err1;
                        }
                    }
                    _DiskCache_PutBlock(self, pBlock);
                }

                if (myLruChainGeneration != self->lruChainGeneration) {
                    done = false;
                    break;
                }
            );
        }
    }
    Lock_Unlock(&self->interlock);
}

// Auto syncs cache blocks to their associated disks
static void _DiskCache_AutoSync(DiskCacheRef _Nonnull self)
{
    DiskCache_Sync(self, NULL, kMediaId_Current);
    _DiskCache_ScheduleAutoSync(self);
}

// Schedule an automatic sync of cached blocks to the disk(s)
static void _DiskCache_ScheduleAutoSync(DiskCacheRef _Nonnull self)
{
    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, TimeInterval_MakeSeconds(30));

    try_bang(DispatchQueue_DispatchAsyncAfter(self->autoSyncQueue, deadline, (VoidFunc_1) _DiskCache_AutoSync, self, 0));
}
