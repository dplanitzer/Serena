//
//  DiskCache.c
//  kernel
//
//  Created by Dietmar Planitzer on 11/6/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "DiskCachePriv.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <log/Log.h>
#include <ext/bit.h>
#include <ext/timespec.h>
#include <kern/kalloc.h>
#include <sched/vcpu.h>


DiskCacheRef _Nonnull  gDiskCache;

errno_t DiskCache_Create(size_t blockSize, size_t maxBlockCount, DiskCacheRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DiskCache* self;
    
    assert(__ELAST <= UCHAR_MAX);
    assert(blockSize > 0 && ispow2_sz(blockSize));
    assert(maxBlockCount > 0);
    
    try(kalloc_cleared(sizeof(DiskCache), (void**) &self));

    mtx_init(&self->interlock);
    cnd_init(&self->condition);

    self->nextAvailSessionId = 1;
    self->blockSize = blockSize;
    self->blockCount = 0;
    self->blockCapacity = maxBlockCount;
    assert(self->blockCapacity > 0);

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
errno_t _DiskCache_LockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, LockMode mode)
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

        err = cnd_wait(&self->condition, &self->interlock);
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
void _DiskCache_UnlockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
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

    cnd_broadcast(&self->condition);
}

#if 0
// Upgrades the given block content lock from being locked shared to locked
// exclusive. Expects that the caller is holding a shared lock on the block
errno_t _DiskCache_UpgradeBlockContentLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    decl_try_err();

    assert(pBlock->shareCount > 0);

    while (pBlock->shareCount > 1 && pBlock->flags.exclusive) {
        err = cnd_wait(&self->condition, &self->interlock);
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
void _DiskCache_DowngradeBlockContentLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    ASSERT_LOCKED_EXCLUSIVE(pBlock);

    pBlock->flags.exclusive = 0;
    pBlock->shareCount++;

    // Purposefully do not give other clients who are waiting to be able to lock
    // this block exclusively a chance to do so. First, we want to do the
    // exclusive -> shared transition atomically and second none else would be
    // able to lock exclusively anyway since we now own the lock shared
}


static void _DiskCache_RegisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    deque_t* chain = &self->diskAddrHash[idx];

    deque_add_first(chain, &pBlock->hashNode);
    deque_add_first(&self->lruChain, &pBlock->lruNode);
    self->lruChainGeneration++;
}

static void _DiskCache_DeregisterBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    const size_t idx = DiskBlock_Hash(pBlock) & DISK_BLOCK_HASH_CHAIN_MASK;
    deque_t* chain = &self->diskAddrHash[idx];

    deque_remove(chain, &pBlock->hashNode);
    deque_remove(&self->lruChain, &pBlock->lruNode);
    self->lruChainGeneration++;
}
#if 0
void _DiskCache_Print(DiskCacheRef _Nonnull _Locked self)
{
    printf("{");
    for (size_t i = 0; i < DISK_BLOCK_HASH_CHAIN_COUNT; i++) {
        deque_for_each(&self->diskAddrHash[i], DiskBlock,
            printf("%u [%u], ", pCurNode->lba, i);
        );
    }
    printf("}");
}

void _DiskCache_PrintLruChain(DiskCacheRef _Nonnull _Locked self)
{
    printf("{");
    deque_for_each(&self->lruChain, deque_node_t,
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);
        printf("%u", pb->lba);
        if (pCurNode->next) {
            printf(", ");
        }
    );
    printf("}");
}
#endif


static errno_t _DiskCache_CreateBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, blkno_t lba, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    // We can still grow the disk block list
    err = DiskBlock_Create(s->sessionId, lba, self->blockSize, &pBlock);
    if (err == EOK) {
        _DiskCache_RegisterBlock(self, pBlock);
        self->blockCount++;
    }
    *pOutBlock = pBlock;

    return err;
}

// Finds the oldest cached block that isn't currently in use and re-targets this
// block to the new disk address.  
static DiskBlockRef _DiskCache_ReuseCachedBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, blkno_t lba)
{
    DiskBlockRef pBlock = NULL;

    deque_for_each_reversed(&self->lruChain, deque_node_t, 
        DiskBlockRef pb = DiskBlockFromLruChainPointer(pCurNode);

        //XXX we previously allowed the reuse of a dirty block and we would sync out the
        // dirty block before reuse. However we currently don't have access to the session
        // that owns this block. Thus this is disabled for now.
        //if (!DiskBlock_InUse(pb) && (!pb->flags.isDirty || (pb->flags.isDirty && !pb->flags.isPinned))) {
        if (!DiskBlock_InUse(pb) && !pb->flags.isDirty && !pb->flags.isPinned) {
            pBlock = pb;
            break;
        }
    );

    if (pBlock) {
        // Sync the block to disk if necessary
        //XXX see above
        //_DiskCache_SyncBlock(self, pBlock);

        _DiskCache_DeregisterBlock(self, pBlock);
        DiskBlock_SetDiskAddress(pBlock, s->sessionId, lba);
        DiskBlock_PurgeData(pBlock, self->blockSize);
        _DiskCache_RegisterBlock(self, pBlock);
    }

    return pBlock;
}

// Returns the block that corresponds to the disk address (sessionId, lba).
// A new block is created if needed or an existing block is retrieved from the
// cached list of blocks. The caller must lock the content of the block before
// doing I/O on it or before handing it to a filesystem.
errno_t _DiskCache_GetBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, blkno_t lba, unsigned int options, DiskBlockRef _Nullable * _Nonnull pOutBlock)
{
    decl_try_err();
    DiskBlockRef pBlock;

    for (;;) {
        // Look up the block based on (sessionId, lba)
        const size_t idx = DiskBlock_HashKey(s->sessionId, lba) & DISK_BLOCK_HASH_CHAIN_MASK;
        deque_t* chain = &self->diskAddrHash[idx];
    
        pBlock = NULL;
        deque_for_each(chain, DiskBlock,
            if (DiskBlock_IsEqualKey(pCurNode, s->sessionId, lba)) {
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
            err = _DiskCache_CreateBlock(self, s, lba, &pBlock);
            break;
        }
        else {
            // We can't create any more disk blocks. Try to reuse one that isn't
            // currently in use. We may have to wait for a disk block to become
            // available for use if they are all currently in use.
            pBlock = _DiskCache_ReuseCachedBlock(self, s, lba);
            if (pBlock) {
                break;
            }

            try_bang(cnd_wait(&self->condition, &self->interlock));
        }
    }


    if (pBlock) {
        if ((options & kGetBlock_Exclusive) == kGetBlock_Exclusive && DiskBlock_InUse(pBlock)) {
            pBlock = NULL;
        }

        if (pBlock) {
            if ((options & kGetBlock_RecentUse) == kGetBlock_RecentUse) {
                deque_remove(&self->lruChain, &pBlock->lruNode);
                deque_add_first(&self->lruChain, &pBlock->lruNode);
                self->lruChainGeneration++;
            }
        }
    }

    *pOutBlock = pBlock;
    return err;
}

void _DiskCache_PutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock)
{
    if (!DiskBlock_InUse(pBlock)) {
        assert(pBlock->flags.op == kDiskBlockOp_Idle);

        // Wake the wait() in _DiskCache_GetBlock()
        cnd_broadcast(&self->condition);
    }
}

void _DiskCache_UnlockContentAndPutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock)
{
    _DiskCache_UnlockBlockContent(self, pBlock);
    _DiskCache_PutBlock(self, pBlock);
}


//
// API
//

size_t DiskCache_GetBlockSize(DiskCacheRef _Nonnull self)
{
    return self->blockSize;
}
