//
//  DiskCachePriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/13/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskCachePriv_h
#define DiskCachePriv_h

#include "DiskCache.h"
#include "DiskBlock.h"
#include <klib/Hash.h>
#include <klib/List.h>
#include <dispatcher/ConditionVariable.h>
#include <driver/disk/DiskDriver.h>
#include <sched/mtx.h>


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
// * the state of a block is protected by the interlock
// * the content of a block is protected by a shared/exclusive lock
// * the block shared/exclusive lock is an extension of the interlock
//    - thus the shared/exclusive lock can not change state as long as someone
//      holds the interlock
// * a block counts as being in use if it's content is locked shared or exclusive
// * there are three block use scenarios:
//    - prefetch: block content is being prefetched for later use
//    - acquired: block is being used by a filesystem
//    - writeback: block content was changed earlier and is now being written
//                 back to disk
// * each one of the three block use scenarios counts as using a block and thus
//   the block content must be locked before the use begins and unlocked after
//   the use has ended
// * prefetch and writeback are exclusive in the sense that they are only
//   executed if the blocks isn't already in use (ie acquired)
// * acquisition is the most flexible use in the sense that it is the only use
//   that allows multiple users at a time. Ie multiple filesystems may read from
//   the same block at the same time
// * note that you can acquire a disk block for read-only while it is doing a
//   writeback to disk
//
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
// Cache operations:
// * Acquire for read-only
//      - if hasData
//          - lock content shared
//          - use data
//          - unlock content (shared)
//      - if !hasData
//          - lock content exclusive
//          - do sync I/O read
//          - downgrade content lock to shared
//          - use data
//          - unlock content (shared)
//
// * Acquire for sync-write
//      - lock content exclusive
//      - modify data
//      - downgrade content lock to shared
//      - do sync I/O write
//      - unlock content (shared)
//
// * Acquire for deferred-write
//      - lock content exclusive
//      - modify data
//      - mark as dirty
//      - unlock content (exclusive)
//
// * Prefetch (async-read) block
//      - if !hasData
//          - lock content exclusive
//          - do async I/O read
//          - unlock content (exclusive)
//
// * Sync dirty block (sync-write)
//      - if isCached (useCount == 0) && isDirty
//          - lock content shared
//          - sync I/O write
//          - unlock content (shared)
// 

#if DEBUG
#define ASSERT_LOCKED_EXCLUSIVE(__block) assert((__block)->flags.exclusive == 1)
#define ASSERT_LOCKED_SHARED(__block) assert((__block)->shareCount > 0 && (__block)->flags.exclusive == 0)
#else
#define REQUIRE_LOCKED_EXCLUSIVE(__block)
#define ASSERT_LOCKED_SHARED(__block)
#endif


// Locking modes for the block content lock
typedef enum LockMode {
    kLockMode_Shared,
    kLockMode_Exclusive
} LockMode;


// _DiskCache_GetBlock() options
enum {
    kGetBlock_RecentUse = 1,        // Count this GetBlock() as a recent use and adjust the LRU chain accordingly
    kGetBlock_Allocate = 2,         // Allocate a disk block with the given address if there isn't already a block with this address in the cache
    kGetBlock_Exclusive = 4,        // Only return the requested block if it isn't in use
};


#define DISK_BLOCK_HASH_CHAIN_COUNT         8
#define DISK_BLOCK_HASH_CHAIN_MASK          (DISK_BLOCK_HASH_CHAIN_COUNT - 1)


typedef struct DiskCache {
    mtx_t                       interlock;
    ConditionVariable           condition;
    int                         nextAvailSessionId;
    size_t                      lruChainGeneration;     // Incremented every time the LRU chain is modified
    List/*<DiskBlock>*/         lruChain;               // Cached disk blocks stored in a LRU chain; first -> most recently used; last -> least recently used
    size_t                      blockSize;
    size_t                      blockCount;             // Number of disk blocks owned and managed by the disk cache (blocks in use + blocks held on the cache lru chain)
    size_t                      blockCapacity;          // Maximum number of disk blocks that may exist at any given time
    size_t                      dirtyBlockCount;        // Number of blocks in the cache that are currently marked dirty
    List/*<DiskBlock>*/         diskAddrHash[DISK_BLOCK_HASH_CHAIN_COUNT];  // Hash table organizing disk blocks by disk address
} DiskCache;


#define DiskBlockFromLruChainPointer(__ptr) \
(DiskBlockRef) (((uint8_t*)__ptr) - offsetof(struct DiskBlock, lruNode))


extern errno_t _DiskCache_LockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock, LockMode mode);
extern void _DiskCache_UnlockBlockContent(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock);
extern void _DiskCache_DowngradeBlockContentLock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock);

extern errno_t _DiskCache_GetBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, blkno_t lba, unsigned int options, DiskBlockRef _Nullable * _Nonnull pOutBlock);
extern void _DiskCache_PutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull pBlock);
extern void _DiskCache_UnlockContentAndPutBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nullable pBlock);

extern errno_t _DiskCache_SyncBlock(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef pBlock);

extern errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, const DiskSession* _Nonnull s, DiskBlockRef _Nonnull pBlock, DiskBlockOp op, bool isSync);

extern void DiskCache_OnDiskRequestDone(DiskCacheRef _Nonnull self, StrategyRequest* _Nonnull req);

#endif /* DiskCachePriv_h */
