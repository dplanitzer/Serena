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

// Define to force all writes to be synchronous
//#define __FORCE_WRITES_SYNC 1

//
// This is a massively concurrent disk cache implementation. Meaning, an important
// goal here is to allow as much parallism as possible:
//
// * multiple processes should be able to read from a cached block at the same
//   time
// * a process should be able to read from a block that is in the process of
//   being written to disk
//
//
// Assumptions, rules, etc:
//
// * a block has a use count. A block is in use as long as the use count > 0
// * a block that's sitting passively in the cache has a use count == 0
// * a block may not be reused while it is in use
// * conversely, a block may be reused for another disk address while its use
//   count == 0
// * a block may be locked for shared or exclusive operations as long as its
//   use count > 0
// * conversely, a block may not be locked exclusive or shared while its use
//   count is == 0
// * you have to increment the use count on a block if you want to use it
// * you have to decrement the user count when you are done with the block
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
// * multiple clients can lock a block for shared use. No client can lock for
//   exclusive use as long as there is at least one shared lock on the block
// * a disk read operation requires exclusive locking throughout
// * a disk write operation:
//     - requires exclusive locking to kick it off (modifying state to start write)
//     - is changed to shared locking while the actual disk write is happening
//     - is changed back to exclusive locking to update the block state at the
//       end of the I/O
// * acquiring a block for read-only requires that the block is locked shared
//   (however, the block is initially locked exclusive if the data must be
//   retrieved first. The lock is downgraded to shared once the data is available)
// * acquiring a block for modifications requires that it is locked exclusively
// * it is not possible for two block read operations to happen at the same time
//   for the same block since a block read requires exclusive locking
// * it is not possible for a block read and block write to happen at the same
//   time for the same block since block reads require exclusive locking
// * it is theoretically possible for a second write to get triggered while a
//   write is already happening on a block. The second write simply joins the
//   already ongoing write in this case and no new disk write is triggered. This
//   is safe since the block data can not have been changed between those two
//   writes since modifying the bloc is only possible if locking the block
//   exclusively. However locking the block exclusively is only possible after
//   all shared lock holders have dropped their lock and no other exclusive lock
//   holder exists
// * a blocks's isDirty can not be true if hasData is false 
// 

#if DEBUG
#define ASSERT_LOCKED_EXCLUSIVE(__block) assert((__block)->flags.exclusive == 1)
#define ASSERT_LOCKED_SHARED(__block) assert((__block)->shareCount > 0 && (__block)->flags.exclusive == 0)
#else
#define REQUIRE_LOCKED_EXCLUSIVE(__block)
#define ASSERT_LOCKED_SHARED(__block)
#endif

static errno_t _DiskCache_FlushBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef pBlock);
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
    DiskBlockRef        emptyBlock;             // Single, shared empty block (for read only access)
    size_t              lruChainGeneration;     // Incremented every time the LRU chain is modified
    List                lruChain;               // Cached disk blocks stored in a LRU chain; first -> most recently used; last -> least recently used
    size_t              blockCount;             // Number of disk blocks owned and managed by the disk cache (blocks in use + blocks held on the cache lru chain)
    size_t              blockCapacity;          // Maximum number of disk blocks that may exist at any given time
    size_t              dirtyBlockCount;        // Number of blocks in the cache that are currently marked dirty
    List                diskAddrHash[DISK_BLOCK_HASH_CHAIN_COUNT];  // Hash table organizing disk blocks by disk address
} DiskCache;



DiskCacheRef _Nonnull  gDiskCache;

errno_t DiskCache_Create(const SystemDescription* _Nonnull pSysDesc, DiskCacheRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskCache* self;
    
    try(kalloc_cleared(sizeof(DiskCache), (void**) &self));
    try(DiskBlock_Create(kDiskId_None, kMediaId_None, 0, &self->emptyBlock));

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

// Upgrades the given block lock from being locked shared to locked exclusive.
// Expects that the caller is holding a shared lock on the block
static errno_t _DiskCache_UpgradeBlockLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
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

// Downgrades the given block from exclusive lock mode to shared lock mode.
// Expects that the caller is holding an exclusive lock on the block.
static void _DiskCache_DowngradeBlockLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
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

// Looks up the disk block registered for the disk address (diskId, mediaId, lba)
// and returns it with a use_count + 1. Returns NULL if no such block is found. 
static DiskBlock* _Nullable _DiskCache_FindBlock(DiskCacheRef _Nonnull _Locked self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba)
{
    const size_t idx = DiskBlock_HashKey(diskId, mediaId, lba) & DISK_BLOCK_HASH_CHAIN_MASK;
    List* chain = &self->diskAddrHash[idx];
    
    List_ForEach(chain, DiskBlock,
        if (DiskBlock_IsEqualKey(pCurNode, diskId, mediaId, lba)) {
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
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);
        print("%u", pb->lba);
        if (pCurNode->next) {
            print(", ");
        }
    );
    print("}");
}


static errno_t _DiskCache_CreateBlock(DiskCacheRef _Nonnull _Locked self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    // We can still grow the disk block list
    err = DiskBlock_Create(diskId, mediaId, lba, &pBlock);
    if (err == EOK) {
        _DiskCache_RegisterBlock(self, pBlock);
        self->blockCount++;
    }
    *pOutBlock = pBlock;

    return err;
}

// Finds the oldest cached block that isn't currently in use and re-targets this
// block to the new disk address.  
static DiskBlockRef _DiskCache_ReuseCachedBlock(DiskCacheRef _Nonnull _Locked self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba)
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
        // Flush the block to disk if necessary
        _DiskCache_FlushBlock(self, pBlock);

        _DiskCache_UnregisterBlock(self, pBlock);
        DiskBlock_SetTarget(pBlock, diskId, mediaId, lba);
        _DiskCache_RegisterBlock(self, pBlock);
    }

    return pBlock;
}

// Returns the block that corresponds to the disk address (diskId, mediaId, lba).
// A new block is created if needed or an existing block is retrieved from the
// cached list of blocks. The caller must lock the block before doing anything
// with it.
static errno_t _DiskCache_GetBlock(DiskCacheRef _Nonnull _Locked self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

retry:
    pBlock = _DiskCache_FindBlock(self, diskId, mediaId, lba);
    if (pBlock == NULL) {
        if (self->blockCount < self->blockCapacity) {
            // We can still grow the disk block list
            try(_DiskCache_CreateBlock(self, diskId, mediaId, lba, &pBlock));
        }
        else {
            // We can't create any more disk blocks. Try to reuse one that isn't
            // currently in use. We may have to wait for a disk block to become
            // available for use if they are all currently in use.
            pBlock = _DiskCache_ReuseCachedBlock(self, diskId, mediaId, lba);
            if (pBlock == NULL) {
                try(ConditionVariable_Wait(&self->condition, &self->interlock, kTimeInterval_Infinity));
                goto retry;
            }
        }
    }

    DiskBlock_BeginUse(pBlock);
    List_Remove(&self->lruChain, &pBlock->lruNode);
    List_InsertBeforeFirst(&self->lruChain, &pBlock->lruNode);
    self->lruChainGeneration++;

catch:
    *pOutBlock = pBlock;
    return err;
}

static void _DiskCache_PutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    DiskBlock_EndUse(pBlock);

    if (!DiskBlock_InUse(pBlock)) {
        assert(pBlock->flags.op == kDiskBlockOp_Idle);
        assert(pBlock->flags.exclusive == 0 && pBlock->shareCount == 0);

        // Wake the wait() in _DiskBlock_Get() if this isn't the (singleton) empty block
        if (pBlock != self->emptyBlock) {
            ConditionVariable_Broadcast(&self->condition);
        }
    }
}

// Same as _DiskCache_GetBlock() but also locks the block with the requested
// mode. The block is returned locked.
static errno_t _DiskCache_GetAndLockBlock(DiskCacheRef _Nonnull _Locked self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, LockMode mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    err = _DiskCache_GetBlock(self, diskId, mediaId, lba, &pBlock);
    if (err == EOK) {
        err = _DiskCache_LockBlock(self, pBlock, mode);
        if (err != EOK) {
            _DiskCache_PutBlock(self, pBlock);
            pBlock = NULL;
        }
    }

    *pOutBlock = pBlock;
    return err;
}

static void _DiskCache_UnlockAndPutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock)
{
    _DiskCache_UnlockBlock(self, pBlock);
    _DiskCache_PutBlock(self, pBlock);
}


//
// API
//

// Triggers an asynchronous loading of the disk block data at the address
// (diskId, mediaId, lba)
errno_t DiskCache_PrefetchBlock(DiskCacheRef _Nonnull self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Can not address blocks on a disk or media that doesn't exist
    if (diskId == kDiskId_None) {
        return ENODEV;
    }
    else if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Get the block
    err = _DiskCache_GetAndLockBlock(self, diskId, mediaId, lba, kLockMode_Shared, &pBlock);
    if (err == EOK && !pBlock->flags.hasData) {
        // Upgrade the lock to exclusive and trigger an async read since this
        // block has no data
        ASSERT_LOCKED_SHARED(pBlock);
        err = _DiskCache_UpgradeBlockLock(self, pBlock);
        if (err == EOK) {
            // Trigger the async read. Note that endIO() will unlock-and-put the
            // block for us.
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, false);
        }
    }
    if (err != EOK && pBlock) {
        _DiskCache_UnlockAndPutBlock(self, pBlock);
    }

    Lock_Unlock(&self->interlock);

    return err;
}

// Check whether the given block has dirty data and write it synchronously to
// disk, if so.
static errno_t _DiskCache_FlushBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef pBlock)
{
    decl_try_err();

    err = _DiskCache_LockBlock(self, pBlock, kLockMode_Shared);
    if (err == EOK && pBlock->flags.isDirty) {
        ASSERT_LOCKED_SHARED(pBlock);
        err = _DiskCache_UpgradeBlockLock(self, pBlock);
        if (err == EOK) {
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
        }
        _DiskCache_UnlockBlock(self, pBlock);
    }

    return err;
}

errno_t DiskCache_FlushBlock(DiskCacheRef _Nonnull self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Can not address blocks on a disk or media that doesn't exist
    if (diskId == kDiskId_None) {
        return ENODEV;
    }
    else if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Get the block
    err = _DiskCache_GetBlock(self, diskId, mediaId, lba, &pBlock);
    if (err == EOK) {
        err = _DiskCache_FlushBlock(self, pBlock);
        _DiskCache_PutBlock(self, pBlock);
    }

    Lock_Unlock(&self->interlock);

    return err;
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

errno_t DiskCache_AcquireBlock(DiskCacheRef _Nonnull self, DiskId diskId, MediaId mediaId, LogicalBlockAddress lba, AcquireBlock mode, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock = NULL;

    // Can not address blocks on a disk or media that doesn't exist
    if (diskId == kDiskId_None) {
        return ENODEV;
    }
    else if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }


    Lock_Lock(&self->interlock);

    // Get and lock the block. Lock mode depends on whether the block already
    // has data or not and whether the acquisition mode indicates that the
    // caller wants to modify the block contents or not.
    const LockMode lockMode = (mode == kAcquireBlock_ReadOnly) ? kLockMode_Shared : kLockMode_Exclusive;
    try(_DiskCache_GetAndLockBlock(self, diskId, mediaId, lba, lockMode, &pBlock));


    // Check whether we want to modify the block contents and whether the block
    // is dirty. If so, write the current block data to the disk first. We do
    // this synchronously.
    if (lockMode == kLockMode_Exclusive && pBlock->flags.isDirty) {
        try(_DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true));
    }


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
            // We always clear the block data because we don't know whether the
            // data is all zero or not
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            memset(pBlock->data, 0, pBlock->flags.byteSize);
            pBlock->flags.hasData = 1;
            break;

        case kAcquireBlock_Replace:
            // Caller accepts whatever is currently in the buffer since it's
            // going to replace every byte anyway.
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            pBlock->flags.hasData = 1;
            break;

        case kAcquireBlock_Update:
            ASSERT_LOCKED_EXCLUSIVE(pBlock);
            if (!pBlock->flags.hasData) {
                try(_DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, true));
            }
            break;

        case kAcquireBlock_ReadOnly:
            if (!pBlock->flags.hasData) {
                ASSERT_LOCKED_SHARED(pBlock);
                try(_DiskCache_UpgradeBlockLock(self, pBlock));
                try(_DiskCache_DoIO(self, pBlock, kDiskBlockOp_Read, true));
                _DiskCache_DowngradeBlockLock(self, pBlock);
            }
            break;

        default:
            abort();
            break;
    }

catch:
    if (err != EOK && pBlock) {
        _DiskCache_UnlockAndPutBlock(self, pBlock);
        pBlock = NULL;
    }

    Lock_Unlock(&self->interlock);
    *pOutBlock = pBlock;

    return err;
}


void DiskCache_RelinquishBlock(DiskCacheRef _Nonnull self, DiskBlockRef _Nullable pBlock)
{
    if (pBlock) {
        Lock_Lock(&self->interlock);
        _DiskCache_UnlockAndPutBlock(self, pBlock);
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
#if defined(__FORCE_WRITES_SYNC)
    mode = kWriteBlock_Sync;
#endif

    Lock_Lock(&self->interlock);

    // We must be holding the exclusive lock here
    ASSERT_LOCKED_EXCLUSIVE(pBlock);

    switch (mode) {
        case kWriteBlock_Sync:
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, true);
            break;

        case kWriteBlock_Async:
            err = _DiskCache_DoIO(self, pBlock, kDiskBlockOp_Write, false);
            break;

        case kWriteBlock_Deferred:
            if (pBlock->flags.isDirty == 0) {
                pBlock->flags.isDirty = 1;
                self->dirtyBlockCount++;
                // Block data will be written out when needed
            }
            break;

        default:
            abort(); break;
    }

    _DiskCache_UnlockAndPutBlock(self, pBlock);

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

    return pBlock->status;
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

    // Assert that we were called with the lock held in exclusive mode
    ASSERT_LOCKED_EXCLUSIVE(pBlock);


    // Just join an already ongoing I/O operation if one is active (and of the
    // same type as 'op')
    if (pBlock->flags.op == op) {
        return (isSync) ? _DiskCache_WaitIO(self, pBlock, op) : EOK;
    }


    // Start a new I/O operation
    DiskDriverRef pDriver = (DiskDriverRef) DriverCatalog_CopyDriverForDriverId(gDriverCatalog, pBlock->diskId);
    if (pDriver == NULL) {
        return ENODEV;
    }

    pBlock->flags.op = op;
    pBlock->flags.async = (isSync) ? 0 : 1;
    pBlock->status = EOK;

    if (op == kDiskBlockOp_Write) {
        _DiskCache_DowngradeBlockLock(self, pBlock);
    }

    err = DiskDriver_BeginIO(pDriver, pBlock);
    if (err == EOK && isSync) {
        err = _DiskCache_WaitIO(self, pBlock, op);
        // The lock is now held in exclusive mode again, if succeeded
    }
    if (err != EOK && op == kDiskBlockOp_Write) {
        ASSERT_LOCKED_SHARED(pBlock);
        try_bang(_DiskCache_UpgradeBlockLock(self, pBlock));
    }


    // Return with the lock held exclusively if this is a synchronous I/O op
#if DEBUG
    if (isSync) {
        ASSERT_LOCKED_EXCLUSIVE(pBlock);
    }
#endif

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
void DiskCache_OnBlockFinishedIO(DiskCacheRef _Nonnull self, DiskDriverRef pDriver, DiskBlockRef _Nonnull pBlock, errno_t status)
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
            try_bang(_DiskCache_UpgradeBlockLock(self, pBlock));
            // Switched to exclusive locking here
            if (status == EOK && pBlock->flags.isDirty) {
                pBlock->flags.isDirty = 0;
                self->dirtyBlockCount--;
            }
            break;

        default:
            abort();
            break;
    }

    pBlock->flags.op = kDiskBlockOp_Idle;
    pBlock->flags.async = 0;
    pBlock->status = status;

    if (isAsync) {
        // Drops exclusive lock if this is a read op
        // Drops shared lock if this is a write op
        _DiskCache_UnlockAndPutBlock(self, pBlock);
        // Unlocked here 
    }
    else {
        // Wake up WaitIO()
        ConditionVariable_Broadcast(&self->condition);
        // Will return with the lock held in exclusive mode
    }

    Lock_Unlock(&self->interlock);

    // Balance the retain from DoIO()
    Object_Release(pDriver);
}


// Synchronously flushes all cached and unwritten disk block for drive 'diskId'
// and media 'mediaId', to disk. Does nothing if either value is kXXX_None. 
errno_t DiskCache_Flush(DiskCacheRef _Nonnull self, DiskId diskId, MediaId mediaId)
{
    decl_try_err();

    if (diskId == kDiskId_None) {
        return ENODEV;
    }
    else if (mediaId == kMediaId_None) {
        return ENOMEDIUM;
    }

    Lock_Lock(&self->interlock);
    if (self->dirtyBlockCount > 0) {
        size_t myLruChainGeneration = self->lruChainGeneration;
        bool loop = true;

        // We push dirty blocks to the disk, starting at the block that has been
        // sitting dirty in memory for the longest time. Note that we drop the
        // interlock while writing the block to the disk. This is why we need to
        // check here whether the LRU chain has changed on us while we were unlocked.
        // If so, we restart the iteration.
        while (loop) {
            List_ForEachReversed(&self->lruChain, ListNode, 
                DiskBlockRef pBlock = DiskBlockFromLruChainPointer(pCurNode);

                DiskBlock_BeginUse(pBlock);
                if (pBlock->diskId == diskId && pBlock->mediaId == mediaId) {
                    const errno_t err1 = _DiskCache_FlushBlock(self, pBlock);
                    if (err == EOK) {
                        // Return the first error that we encountered. However,
                        // we continue flushing as many blocks as we can
                        err = err1;
                    }
                }
                DiskBlock_EndUse(pBlock);

                if (myLruChainGeneration != self->lruChainGeneration) {
                    loop = true;
                    break;
                }
                else {
                    loop = false;
                }
            );
        }
    }
    Lock_Unlock(&self->interlock);
}
