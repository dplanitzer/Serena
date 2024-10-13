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


typedef struct FSContainerInfo {
    size_t              blockSize;          // byte size of a logical disk block. A single logical disk block may map to multiple physical blocks. The FSContainer transparently takes care of the mapping
    LogicalBlockCount   blockCount;         // overall number of addressable blocks in this FSContainer
    bool                isReadOnly;         // true if all the data in the FSContainer is hardware write protected 
    bool                isMediaLoaded;      // true if if all the underlying mediums of the FSContainer are physically loaded in their disk drives 
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

    // Reads the contents of the block at index 'lba'. 'buffer' must be big
    // enough to hold the data of a block. Blocks the caller until the read
    // operation has completed. Note that this function will never return a
    // partially read block. Either it succeeds and the full block data is
    // returned, or it fails and no block data is returned.
    // The abstract implementation returns EIO.
    errno_t (*getBlock)(void* _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba);

    // Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
    // must be big enough to hold a full block. Blocks the caller until the
    // write has completed. The contents of the block on disk is left in an
    // indeterminate state of the write fails in the middle of the write. The
    // block may contain a mix of old and new data.
    // The abstract implementation returns EIO.
    errno_t (*putBlock)(void* _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba);
);


//
// Methods for use by FSContainer users.
//

#define FSContainer_GetInfo(__self, __pOutInfo) \
invoke_n(getInfo, FSContainer, __self, __pOutInfo)

#define FSContainer_GetBlock(__self, __pBuffer, __lba) \
invoke_n(getBlock, FSContainer, __self, __pBuffer, __lba)

#define FSContainer_PutBlock(__self, __pBuffer, __lba) \
invoke_n(putBlock, FSContainer, __self, __pBuffer, __lba)

#endif /* FSContainer_h */
