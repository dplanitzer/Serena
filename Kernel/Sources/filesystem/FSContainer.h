//
//  FSContainer.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef FSContainer_h
#define FSContainer_h

#include <klib/klib.h>
#include <kobj/Object.h>
#include <filesystem/FSBlock.h>
#include <System/Filesystem.h>


// A filesystem container provides access to the data on a mass storage device
// or a collection of distinct mass storage devices under the assumption that
// this data represents the persistent state of a filesystem.
open_class(FSContainer, Object,
    size_t              blockSize;          // byte size of a logical disk block. A single logical disk block may map to multiple physical blocks. The FSContainer transparently takes care of the mapping
    LogicalBlockCount   blockCount;         // overall number of addressable blocks in this FSContainer
    MediaId             mediaId;            // media in which the FS container resides
    uint32_t            properties;         // FS properties defined by the FS container 
);
open_class_funcs(FSContainer, Object,

    // Prefetches a block and stores it in the disk cache if possible. The prefetch
    // is executed asynchronously. An error is returned if the prefetch could not
    // be successfully started. Note that the returned error does not indicate
    // whether the read operation as such was successful or not.
    errno_t (*prefetchBlock)(void* _Nonnull self, LogicalBlockAddress lba);

    // Synchronously flushes the block at the logical block address 'lba' to
    // disk if it contains unwritten (dirty) data. Does nothing if the block is
    // clean.
    errno_t (*syncBlock)(void* _Nonnull self, LogicalBlockAddress lba);

    // Acquires the disk block with the block address 'lba'. The acquisition is
    // done according to the acquisition mode 'mode'. An error is returned if
    // the disk block needed to be loaded and loading failed for some reason.
    // Once done with the block, it must be relinquished by calling the
    // relinquishBlock() method.
    errno_t (*mapBlock)(void* _Nonnull self, LogicalBlockAddress lba, MapBlock mode, FSBlock* _Nonnull blk);

    // Unmaps the disk block 'pBlock' and writes the disk block back to
    // disk according to the write back mode 'mode'.
    errno_t (*unmapBlock)(void* _Nonnull self, intptr_t token, WriteBlock mode);

    // Synchronously flushes all cached and unwritten blocks belonging to this
    // FS container to disk(s).
    errno_t (*sync)(void* _Nonnull self);


    // Returns the canonical name of the disk on which this FS container resides.
    // An empty string is returned if the name is unknown.
    errno_t (*getDiskName)(void* _Nonnull self, size_t bufSize, char* _Nonnull buf);
);


//
// Methods for use by FSContainer users.
//

#define FSContainer_GetBlockCount(__self)\
((FSContainerRef)__self)->blockCount

#define FSContainer_GetBlockSize(__self)\
((FSContainerRef)__self)->blockSize

#define FSContainer_GetMediaId(__self)\
((FSContainerRef)__self)->mediaId

#define FSContainer_GetFSProperties(__self)\
((FSContainerRef)__self)->properties

#define FSContainer_IsReadOnly(__self)\
((((FSContainerRef)__self)->properties & kFSProperty_IsReadOnly) == kFSProperty_IsReadOnly ? true : false)


#define FSContainer_PrefetchBlock(__self, __driverId, __mediaId, __lba) \
invoke_n(prefetchBlock, FSContainer, __self, __driverId, __mediaId, __lba)

#define FSContainer_SyncBlock(__self, __driverId, __mediaId, __lba) \
invoke_n(syncBlock, FSContainer, __self, __driverId, __mediaId, __lba)


#define FSContainer_MapBlock(__self, __lba, __mode, __blk) \
invoke_n(mapBlock, FSContainer, __self, __lba, __mode, __blk)

#define FSContainer_UnmapBlock(__self, __token, __mode) \
invoke_n(unmapBlock, FSContainer, __self, __token, __mode)


#define FSContainer_Sync(__self) \
invoke_0(sync, FSContainer, __self)


#define FSContainer_GetDiskName(__self, __bufSize, __buf) \
invoke_n(getDiskName, FSContainer, __self, __bufSize, __buf)


//
// Methods for use by FSContainer subclassers.
//

extern errno_t FSContainer_Create(Class* _Nonnull pClass, MediaId mediaId, LogicalBlockCount blockCount, size_t blockSize, uint32_t properties, FSContainerRef _Nullable * _Nonnull pOutSelf);

#endif /* FSContainer_h */
