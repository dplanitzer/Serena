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
#include <disk/DiskBlock.h>


typedef struct FSContainerInfo {
    size_t              blockSize;          // byte size of a logical disk block. A single logical disk block may map to multiple physical blocks. The FSContainer transparently takes care of the mapping
    LogicalBlockCount   blockCount;         // overall number of addressable blocks in this FSContainer
    MediaId             mediaId;            // Id of the currently loaded media; changes with every media eject and insertion; 0 means no media is loaded 
    bool                isReadOnly;         // true if all the data in the FSContainer is hardware write protected 
} FSContainerInfo;


// A filesystem container represents the logical disk that holds the contents of
// a filesystem. A container may span multiple physical disks/partitions or it
// may just represent a single physical disk/partition.
open_class(FSContainer, Object,
);
open_class_funcs(FSContainer, Object,

    // Returns information about the filesystem container and the underlying disk
    // medium(s).
    errno_t (*getInfo)(void* _Nonnull self, FSContainerInfo* pOutInfo);

    // Acquires the disk block with the block address 'lba'. The acquisition is
    // done according to the acquisition mode 'mode'. An error is returned if
    // the disk block needed to be loaded and loading failed for some reason.
    // Once done with the block, it must be relinquished by calling the
    // relinquishBlock() method.
    errno_t (*acquireBlock)(void* _Nonnull self, LogicalBlockAddress lba, DiskBlockAcquire mode, DiskBlockRef _Nullable * _Nonnull pOutBlock);

    // Relinquishes the disk block 'pBlock' and writes the disk block back to
    // disk according to the write back mode 'mode'.
    errno_t (*relinquishBlock)(void* _Nonnull self, DiskBlockRef _Nullable pBlock, DiskBlockWriteBack mode);
);


//
// Methods for use by FSContainer users.
//

#define FSContainer_GetInfo(__self, __pOutInfo) \
invoke_n(getInfo, FSContainer, __self, __pOutInfo)

#define FSContainer_AcquireBlock(__self, __lba, __mode, __pOutBlock) \
invoke_n(acquireBlock, FSContainer, __self, __lba, __mode, __pOutBlock)

#define FSContainer_RelinquishBlock(__self, __pBlock, __mode) \
invoke_n(relinquishBlock, FSContainer, __self, __pBlock, __mode)

#endif /* FSContainer_h */
