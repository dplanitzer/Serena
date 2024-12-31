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
#include <klib/List.h>
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <dispatchqueue/DispatchQueue.h>
#include <driver/disk/DiskDriver.h>


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

static DiskId _DiskCache_GetNewDiskId(DiskCacheRef _Nonnull _Locked self);
static void _DiskCache_ScheduleAutoSync(DiskCacheRef _Nonnull self);
static errno_t _DiskCache_SyncBlock(DiskCacheRef _Nonnull _Locked self, DiskBlockRef pBlock);
static errno_t _DiskCache_DoIO(DiskCacheRef _Nonnull _Locked self, DiskBlockRef _Nonnull _Locked pBlock, DiskBlockOp op, bool isSync);


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

#define DISK_DRIVER_HASH_CHAIN_COUNT        4
#define DISK_DRIVER_HASH_CHAIN_MASK         (DISK_DRIVER_HASH_CHAIN_COUNT - 1)
#define DISK_DRIVER_HASH_CODE(__diskId)     (__diskId)
#define DISK_DRIVER_HASH_INDEX(__diskId)    (DISK_DRIVER_HASH_CODE(__diskId) & DISK_DRIVER_HASH_CHAIN_MASK)


typedef struct DiskEntry {
    ListNode                chain;
    DiskDriverRef _Nonnull  driver;
    DiskId                  diskId;
} DiskEntry;


typedef struct DiskCache {
    Lock                        interlock;
    ConditionVariable           condition;
    DispatchQueueRef _Nonnull   autoSyncQueue;
    DiskBlockRef _Nonnull       emptyBlock;             // Single, shared empty block (for read only access)
    size_t                      lruChainGeneration;     // Incremented every time the LRU chain is modified
    List/*<DiskBlock>*/         lruChain;               // Cached disk blocks stored in a LRU chain; first -> most recently used; last -> least recently used
    size_t                      blockCount;             // Number of disk blocks owned and managed by the disk cache (blocks in use + blocks held on the cache lru chain)
    size_t                      blockCapacity;          // Maximum number of disk blocks that may exist at any given time
    size_t                      dirtyBlockCount;        // Number of blocks in the cache that are currently marked dirty
    List/*<DiskBlock>*/         diskAddrHash[DISK_BLOCK_HASH_CHAIN_COUNT];  // Hash table organizing disk blocks by disk address
    List/*<DiskEntry>*/         driverHash[DISK_DRIVER_HASH_CHAIN_COUNT];   // Hash table mapping DiskId -> DiskDriverRef
    DiskId                      nextDiskId;
} DiskCache;

#endif /* DiskCachePriv_h */
