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

//
// This is a massively concurrent disk cache implementation. Meaning, an important
// goal here is to allow as much parallism as possible:
//
// * multiple processes should be able to read from a block at the same time
// * a process should be able to read from a block that is in the process of
//   being written to disk 
//
// Assumptions, rules, etc:
//
// * a block has a use count. A block is in use as long as the use count > 0
// * a block may not be reused while it is in use
// * conversely, a block may be reused for another disk address while its use
//   count == 0
// * a block may be locked for shared or exclusive operations as long as its
//   use count > 0
// * conversely, a block may not be locked exclusive or shared while its use
//   count is == 0
// * you have to increment the use count on a block if you want to use it
// * you have to decrement the user count when you are done with teh block
// * using a block means:
//    - you want to acquire it
//    - it should be read from disk
//    - its data should be initialized to 0
//    - it should be written to disk
// * every disk block is on the 'disk address hash chain'. This is a hash table
//   that organizes disk blocks by their disk address
// * a disk address is the tuple (driver-id, media-id, lba)
// * every disk block is additionally on a LRU chain
// * doing a _DiskCache_GetBlock() marks the block for use and moves it to the
//   front of the LRU chain
// * doing a _DiskCache_PutBlock() ends the use of a block. It does not change
//   its position in the LRU chain
// * disk blocks are reused beginning from the end of the LRU chain
//
// * at most one client is able to lock a disk block for exclusive use. No-one
//   else can lock exclusively or shared while this client is holding the
//   exclusive lock
// * multiple clients can locks a block for shared use. No client can lock for
//   exclusive use as long as there is at least one shared lock on the block
// * a read operation requires exclusive locking
// * a write operation requires shared locking
// * acquiring a block for read-only requires that the block is locked shared
//   (however, the block is initially locked exclusive if the data must be
//   retrieved first. The lock is downgraded to shared once the data is available)
// * acquiring a block for modifications requires that it is locked exclusively
//

static void _DiskCache_RelinquishBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock);
static errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull _Locked pBlock, DiskBlockOp op, bool isSync);


typedef enum LockMode {
    kLockMode_Shared,
    kLockMode_Exclusive
} LockMode;


#define DISK_BLOCK_HASH_CHAIN_COUNT     8
#define DISK_BLOCK_HASH_CHAIN_MASK      (DISK_BLOCK_HASH_CHAIN_COUNT - 1)

typedef struct DiskCache {
    Lock                interlock;
    ConditionVariable   condition;
    DiskBlockRef        emptyBlock;     // Single, shared empty block (for read only access)
    List                lruChain;       // Cached disk blocks stored in a LRU chain; first -> most recently used; last -> least recently used
    size_t              blockCount;     // Number of disk blocks owned and managed by the disk cache (blocks in use + blocks held on the cache lru chain)
    size_t              blockCapacity;  // Maximum number of disk blocks that may exist at any given time
    List                diskAddrHash[DISK_BLOCK_HASH_CHAIN_COUNT];  // Hash table organizing disk blocks by disk address
} DiskCache;



DiskCacheRef _Nonnull  gDiskCache;

errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskCache* self;
    
    try(kalloc_cleared(sizeof(DiskCache), (void**) &self));
    try(DiskBlock_Create(kDriverId_None, kMediaId_None, 0, &self->emptyBlock));

    Lock_Init(&self->interlock);
    ConditionVariable_Init(&self->condition);
    List_Init(&self->lruChain);
    self->blockCount = 0;
    self->blockCapacity = SystemDescription_GetRamSize(pSysDesc) >> 5;
    assert(self->blockCapacity > 0);

    for (int i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_Init(&self->diskAddrHash[i]);
    }

    *pOutSelf = self;
    return EOK;

catch:
    *pOutSelf = NULL;
    return err;
}

// Locks the given block in shared or exclusive mode. Multiple clients may lock
// a block in shared mode but at most one client can lock a block in exclusive
// mode. A block is only lockable in exclusive mode if no other client is locking
// it in shared or exclusive mode.
static errno_t _DiskCache_LockBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, LockMode mode)
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

// Unlock the given block. This function assumes that if the block is currently
// locked exclusively, that the caller is indeed the owner of the block since
// there can only be a single exclusive locker. If the block is locked in shared
// mode instead, then the caller is assumed to be one of the shared block owners.
static void _DiskCache_UnlockBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
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

// Downgrades the given block from exclusive lock mode to shared lock mode.
static void _DiskCache_DowngradeBlockLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    if (pBlock->flags.exclusive == 0) {
        abort();
    }

    pBlock->flags.exclusive = 0;
    pBlock->shareCount++;

    // Purposefully do not give other clients who are waiting to be able to lock
    // this block exclusively a chance to do so. First, we want to do the
    // exclusive -> shared transition atomically and second none else would be
    // able to lock exclusively anyway since we now own the lock shared
}


#define DiskBlockFromCachePointer(__ptr) \
(DiskBlockRef) (((uint8_t*)__ptr) - offsetof(struct DiskBlock, lruNode))

static void _DiskCache_RegisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];

    List_InsertBeforeFirst(chain, &pBlock->hashNode);
    List_InsertBeforeFirst(&self->lruChain, &pBlock->lruNode);
}

static void _DiskCache_UnregisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];

    List_Remove(chain, &pBlock->hashNode);
    List_Remove(&self->lruChain, &pBlock->lruNode);
}

// Looks up the disk block registered for the disk address (driverId, mediaId, lba)
// and returns it with a use_count + 1. Returns NULL if no such block is found. 
static DiskBlock* _Nullable _DiskCache_FindBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    const size_t idx = DiskBlock_HashKey(driverId, mediaId, lba) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];
    
    List_ForEach(chain, DiskBlock,
        if (DiskBlock_IsEqualKey(pCurNode, driverId, mediaId, lba)) {
            return pCurNode;
        }
    );

    return NULL;
}

static void _DiskCache_Print(DiskCacheRef _Nonnull _Locked self)
{
    print("{");
    for (size_t i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        List_ForEach(&self->diskAddrHash[i], DiskBlock,
            print("%u [%u], ", pCurNode->lba, i);
        );
    }
    print("}");
}

static void _DiskCache_PrintLruChain(DiskCacheRef _Nonnull _Locked self)
{
    print("{");
    List_ForEach(&self->lruChain, ListNode,
        DiskBlockRef pb = DiskBlockFromCachePointer(pCurNode);
        print("%u", pb->lba);
        if (pCurNode->next) {
            print(", ");
        }
    );
    print("}");
}


static errno_t _DiskCache_CreateBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    // We can still grow the disk block list
    err = DiskBlock_Create(driverId, mediaId, lba, &pBlock);
    if (err == EOK) {
        _DiskCache_RegisterBlock(self, pBlock);
        self->blockCount++;
    }
    *pOutBlock = pBlock;

    return err;
}

static DiskBlockRef _DiskCache_ReuseCachedBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba)
{
    DiskBlockRef pBlock = NULL;

    List_ForEachReversed(&self->lruChain, ListNode, 
        DiskBlockRef pb = DiskBlockFromCachePointer(pCurNode);

        if (!DiskBlock_InUse(pb)) {
            pBlock = pb;
            break;
        }
    );

    if (pBlock) {
        _DiskCache_UnregisterBlock(self, pBlock);
        DiskBlock_SetTarget(pBlock, driverId, mediaId, lba);
        _DiskCache_RegisterBlock(self, pBlock);
    }

    return pBlock;
}

// Returns the block that corresponds to the disk address (driverId, mediaId, lba).
// A new block is created if needed or an existing block is retrieved from the
// cached list of blocks. The caller must lock the block before doing anything
// with it.
static errno_t _DiskCache_GetBlock(DiskCacheRef _Nonnull _Locked self, DriverId driverId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

retry:
    pBlock = _DiskCache_FindBlock(self, driverId, mediaId, lba);
    if (pBlock == NULL) {
        if (self->blockCount < self->blockCapacity) {
            // We can still grow the disk block list
            try(_DiskCache_CreateBlock(self, driverId, mediaId, lba, &pBlock));
        }
        else {
            // We can't create any more disk blocks. Try to reuse one that isn't
            // currently in use. We may have to wait for a disk block to become
            // available for use if they are all currently in use.
            pBlock = _DiskCache_ReuseCachedBlock(self, driverId, mediaId, lba);
            if (pBlock == NULL) {
                try(ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity));
                goto retry;
            }
        }
    }

    DiskBlock_BeginUse(pBlock);
    List_Remove(&self->lruChain, &pBlock->lruNode);
    List_InsertBeforeFirst(&self->lruChain, &pBlock->lruNode);

catch:
    *pOutBlock = pBlock;
    return err;
}

static void _DiskCache_PutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    DiskBlock_EndUse(pBlock);

    if (!DiskBlock_InUse(pBlock)) {
        assert(pBlock->flags.op == kDiskBlockOp_Idle);
        assert(pBlock->flags.exclusive == 0);
        assert(pBlock->shareCount == 0);

        // Wake the wait() in _DiskBlock_Get() if this isn't the (singleton) empty block
        if (pBlock != self->emptyBlock) {
            ConditionVariable_Broadcast(&self->condition);
        }
    }
}


// Returns an empty block (all data is zero) for read-only operations.
errno_t DiskCache_AcquireEmptyBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();

    Lock_Lock(&self->interlock);
    
    err = _DiskCache_LockBlock(self, self->emptyBlock, kLockMode_Shared);
    if (err == EOK) {
        DiskBlock_BeginUse(self->emptyBlock);
    }

    Lock_Unlock(&self->interlock);
    *pOutBlock = self->emptyBlock;

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


    Lock_Lock(&self->interlock);

    // Get the block
    try(_DiskCache_GetBlock(self, driverId, mediaId, lba, &pBlock));


    // Lock the block. Lock mode depends on whether the block already has data
    // or not and whether the acquisition mode indicates that the caller wants
    // to modify the block contents or not.
    const LockMode lockMode = (mode == kAcquireBlock_ReadOnly && pBlock->flags.hasData) ? kLockMode_Shared : kLockMode_Exclusive;
    _DiskCache_LockBlock(self, pBlock, lockMode);


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
                    _DiskCache_RelinquishBlock(self, pBlock);
                }
                if (mode == kAcquireBlock_ReadOnly) {
                    _DiskCache_DowngradeBlockLock(self, pBlock);
                }
            }
            break;

        default:
            abort();
            break;
    }

catch:
    Lock_Unlock(&self->interlock);
    *pOutBlock = (err == EOK) ? pBlock : NULL;

    return err;
}


static void _DiskCache_RelinquishBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock)
{
    _DiskCache_UnlockBlock(self, pBlock);
    _DiskCache_PutBlock(self, pBlock);
}

void DiskCache_RelinquishBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    if (pBlock) {
        Lock_Lock(&self->interlock);
        _DiskCache_RelinquishBlock(self, pBlock);
        Lock_Unlock(&self->interlock);
    }
}

errno_t DiskCache_RelinquishBlockWriting(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock, WriteBlock mode)
{
    decl_try_err();

    if (pBlock == NULL) {
        return EOK;
    }
    if (pBlock == self->emptyBlock) {
        // The empty block is for reading only
        abort();
    }

    Lock_Lock(&self->interlock);

    switch (mode) {
        case kWriteBlock_Sync:
            _DiskCache_DowngradeBlockLock(self, pBlock);
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
            break;

        case kWriteBlock_Async:
            abort();
            //XXX not yet err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, false);
            break;

        case kWriteBlock_Deferred:
            // XXX mark the block as dirty
            abort();
            break;

        default:
            abort(); break;
    }

    _DiskCache_RelinquishBlock(self, pBlock);

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
    Lock_Lock(&self->interlock);

    if (pBlock->flags.op == kDiskBlockOp_Read && status == EOK) {
        pBlock->flags.hasData = 1;
    }
    pBlock->flags.op = kDiskBlockOp_Idle;
    pBlock->status = status;

    ConditionVariable_BroadcastAndUnlock(&self->condition, &self->interlock);
}


errno_t DiskCache_Flush(DiskCacheRef _Nonnull self)
{
    decl_try_err();

    //XXX
    return err;
}
