//
//  DiskCache.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCache.h"
#include "DiskBlock.h"
#include <klib/List.h>
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <driver/DriverCatalog.h>
#include <driver/disk/DiskDriver.h>

static errno_t _DiskCache_AcquireBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock);
static void _DiskCache_RelinquishBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock, bool doCache);
static errno_t _DiskCache_RelinquishBlockWriting(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, WriteBlock mode);
static errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull _Locked pBlock, DiskBlockOp op, bool isSync);


#define DISK_BLOCK_HASH_CHAIN_COUNT     8
#define DISK_BLOCK_HASH_CHAIN_MASK      (DISK_BLOCK_HASH_CHAIN_COUNT - 1)

typedef struct DiskCache {
    Lock                lock;
    ConditionVariable   condition;
    List                cache;          // Cached disk blocks stored in a LRU chain; first -> most recently used; last -> least recently used
    size_t              cacheCount;     // Number of disk blocks owned and managed by the disk cache
    size_t              cacheCapacity;  // Maximum number of disk blocks that may exist at any given time
    List                hash[DISK_BLOCK_HASH_CHAIN_COUNT];  // disk block hash table
} DiskCache;



DiskCacheRef _Nonnull  gDiskCache;

errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskCache* self;
    
    try(kalloc_cleared(sizeof(DiskCache), (void**) &self));
    Lock_Init(&self->lock);
    ConditionVariable_Init(&self->condition);
    List_Init(&self->cache);
    self->cacheCount = 0;
    self->cacheCapacity = SystemDescription_GetRamSize(pSysDesc) >> 5;
    assert(self->cacheCapacity > 0);
    for (int i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_Init(&self->hash[i]);
    }

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

static void _DiskCache_RegisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->hash[idx];

    List_InsertBeforeFirst(chain, &pBlock->hashNode);
}

static void _DiskCache_DeregisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->hash[idx];

    List_Remove(chain, &pBlock->hashNode);
}

// Looks up the disk block registered for the disk address (driverId, mediaId, lba)
// and returns it with a use_count + 1. Returns NULL if no such block is found. 
static DiskBlock* _Nullable _DiskCache_FindBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    const size_t idx = DiskBlock_HashKey(driverId, mediaId, lba) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->hash[idx];
    
    List_ForEach(chain, DiskBlock,
        if (DiskBlock_IsEqualKey(pCurNode, driverId, mediaId, lba)) {
            pCurNode->useCount++;
            return pCurNode;
        }
    );

    return NULL;
}

static void _DiskCache_Print(DiskCacheRef _Nonnull _Locked self)
{
    print("{");
    for (size_t i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->hash[i], DiskBlock,
            print("%u [%u], ", pCurNode->lba, i);
        );
    }
    print("}");
}


#define DiskBlockFromCachePointer(__ptr) \
(DiskBlockRef) (((uint8_t*)__ptr) - offsetof(struct DiskBlock, lruNode))

static void _DiskCache_CacheBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    pBlock->flags.isCached = 1;
    pBlock->flags.op = kDiskBlockOp_Idle;
    List_InsertBeforeFirst(&self->cache, &pBlock->lruNode);
}

static void _DiskCache_UncacheBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    pBlock->flags.isCached = 0;
    List_Remove(&self->cache, &pBlock->lruNode);
}

static DiskBlockRef _Nullable _DiskCache_AcquireOldestCachedBlock(DiskCacheRef _Nonnull _Locked self)
{
    DiskBlockRef pBlock = NULL;

    if (self->cache.last) {
        pBlock = DiskBlockFromCachePointer(self->cache.last);
        _DiskCache_UncacheBlock(self, pBlock);
    }

    return pBlock;
}

static void _DiskCache_PrintCached(DiskCacheRef _Nonnull _Locked self)
{
    print("{");
    List_ForEach(&self->cache, ListNode,
        DiskBlockRef pb = DiskBlockFromCachePointer(pCurNode);
        print("%u", pb->lba);
        if (pCurNode->next) {
            print(", ");
        }
    );
    print("}");
}


errno_t DiskCache_AcquireEmptyBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    Lock_Lock(&self->lock);
    err = _DiskCache_AcquireBlock(self, kDriverId_None, kMediaId_None, 0, kAcquireBlock_Cleared, &pBlock);
    Lock_Unlock(&self->lock);
    *pOutBlock = pBlock;

    return err;
}

errno_t DiskCache_AcquireBlock(DiskCacheRef _Nonnull self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Can not address blocks on a disk or media that doesn't exist
    if (driverId == kDriverId_None || mediaId == kMediaId_None) {
        return EIO;
    }


    Lock_Lock(&self->lock);

    // Acquire the block. This waits until a read/write that's currently in
    // progress has finished and until no-one else is using this block
    try(_DiskCache_AcquireBlock(self, driverId, mediaId, lba, mode, &pBlock));


    // States:
    // no-data:
    //  read-only:  clear, start read
    //  update:     clear, start read
    //  replace:    clear
    //
    // idle:
    //  read-only:  -
    //  update:     -
    //  replace:    -
    //
    // reading:
    //  read-only:  wait for read to complete
    //  update:     wait for read to complete
    //  replace:    wait for read to complete
    //
    // writing:
    //  read-only:  -
    //  update:     wait for write to complete
    //  replace:    wait for write to completes
    switch (mode) {
        case kAcquireBlock_Cleared:
            memset(pBlock->data, 0, pBlock->flags.byteSize);
            pBlock->flags.hasData = 1;
            break;

        case kAcquireBlock_Replace:
            // Caller accepts whatever is currently in the buffer since it's
            // going to replace every byte anyway.
            pBlock->flags.hasData = 1;
            break;

        case kAcquireBlock_ReadOnly:
        case kAcquireBlock_Update:
            if (!pBlock->flags.hasData) {
                err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, true);
                if (err != EOK) {
                    _DiskCache_RelinquishBlock(self, pBlock, true);
                }
            }
            break;

        default:
            abort();
            break;
    }

catch:
    Lock_Unlock(&self->lock);
    *pOutBlock = (err == EOK) ? pBlock : NULL;

    return err;
}

// Waits until the given block can be acquired for mode 'mode'. Returns EOK on
// success and a suitable error code otherwise. The block is acquired if this
// function returns EOK.
static errno_t _DiskCache_AcquireBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Find the block or create a new one if it isn't already in-core
    pBlock = _DiskCache_FindBlock(self, driverId, mediaId, lba);
    if (pBlock == NULL) {
        if (self->cacheCount < self->cacheCapacity) {
            try(DiskBlock_Create(driverId, mediaId, lba, &pBlock));
            _DiskCache_RegisterBlock(self, pBlock);
            self->cacheCount++;
        }
        else if (self->cache.last) {
            pBlock = DiskBlockFromCachePointer(self->cache.last);
            _DiskCache_UncacheBlock(self, pBlock);
            _DiskCache_DeregisterBlock(self, pBlock);
            DiskBlock_SetTarget(pBlock, driverId, mediaId, lba);
            _DiskCache_RegisterBlock(self, pBlock);
        }
        else {
            abort();
        }
    }
    else if (pBlock->flags.isCached) {
        _DiskCache_UncacheBlock(self, pBlock);
    }


    // Wait for the block to be idle
    while (pBlock->flags.acquired
            || (pBlock->flags.op == kDiskBlockOp_Read)
            || (pBlock->flags.op == kDiskBlockOp_Write && mode != kAcquireBlock_ReadOnly)) {

        try(ConditionVariable_Wait(&self->condition, &self->lock, kTimeInterval_Infinity));
    }

    pBlock->flags.acquired = 1;

catch:
    *pOutBlock = pBlock;
    return err;
}


static void _DiskCache_RelinquishBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock, bool doCache)
{
    pBlock->flags.acquired = 0;

    if (doCache) {
        _DiskCache_CacheBlock(self, pBlock);
    }

    ConditionVariable_Broadcast(&self->condition);
}

void DiskCache_RelinquishBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    if (pBlock) {
        Lock_Lock(&self->lock);
        _DiskCache_RelinquishBlock(self, pBlock, true);
        pBlock->useCount--;
        Lock_Unlock(&self->lock);
    }
}

errno_t DiskCache_RelinquishBlockWriting(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    decl_try_err();
    bool doCache = false;

    if (pBlock == NULL) {
        return EOK;
    }

    Lock_Lock(&self->lock);

    switch (mode) {
        case kWriteBlock_Sync:
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
            doCache = true;
            break;

        case kWriteBlock_Async:
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, false);
            break;

        case kWriteBlock_Deferred:
            // XXX mark the block as dirty
            abort();
            break;

        default:
            abort(); break;
    }

    _DiskCache_RelinquishBlock(self, pBlock, doCache);
    pBlock->useCount--;

    Lock_Unlock(&self->lock);

    return err;
}


// Blocks the caller until the given block has finished the given I/O operation
// type. Expects to be called with the lock held.
static errno_t _DiskCache_WaitIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, DiskBlockOp op)
{
    decl_try_err();

    while (pBlock->flags.op == op) {
        err = ConditionVariable_Wait(&self->condition, &self->lock, kTimeInterval_Infinity);
        if (err != EOK) {
            return err;
        }
    }

    return EOK;
}

// Starts a read operation and waits for it to complete. Note that this function
// leaves the disk block state in whatever state it was when this function was
// called, if the read can not be successfully started. Typically this means that
// the disk block will stay in NoData state. A future acquisition will then trigger
// another read attempt.
static errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync)
{
    decl_try_err();
    DiskDriverRef pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, pBlock->driverId);
    if (pDriver == NULL) {
        return ENODEV;
    }

    pBlock->useCount++; // Will be balanced by EndIO()
    pBlock->flags.op = op;
    pBlock->status = EOK;

    if ((err = DiskDriver_BeginIO(pDriver, pBlock)) == EOK) {
        // Wait for I/O to complete, if necessary
        if (isSync) {
            err = _DiskCache_WaitIO(self, pBlock, op);
            if (err == EOK) {
                err = pBlock->status;
            }
        }
    }

    Object_Release(pDriver);

    return err;
}


// Must be called by the disk driver when it's done with the block
void DiskCache_OnDiskBlockEndedIO(DiskCacheRef _Nonnull self, DiskBlockRef _Nonnull _Locked pBlock, errno_t status)
{
    Lock_Lock(&self->lock);
    pBlock->status = status;

    if (pBlock->flags.op == kDiskBlockOp_Read && status == EOK) {
        pBlock->flags.hasData = 1;
    }
    pBlock->flags.op = kDiskBlockOp_Idle;

    ConditionVariable_Broadcast(&self->condition);
    pBlock->useCount--;
    Lock_Unlock(&self->lock);
    //ConditionVariable_BroadcastAndUnlock(&self->condition, &self->lock);
}


errno_t DiskCache_Flush(DiskCacheRef _Nonnull self)
{
    decl_try_err();

    //XXX
    return err;
}
