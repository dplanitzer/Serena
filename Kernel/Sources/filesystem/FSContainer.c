//
//  FSContainer.c
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "FSContainer.h"


// Returns information about the filesystem container and the underlying disk
// medium(s).
errno_t FSContainer_getInfo(FSContainerRef _Nonnull self, FSContainerInfo* pOutInfo)
{
    pOutInfo->blockSize = 512;
    pOutInfo->blockCount = 0;
    pOutInfo->mediaId = 0;
    pOutInfo->isReadOnly = true;

    return EOK;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t FSContainer_getBlock(FSContainerRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return EIO;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t FSContainer_putBlock(FSContainerRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    return EIO;
}


class_func_defs(FSContainer, Object,
func_def(getInfo, FSContainer)
func_def(getBlock, FSContainer)
func_def(putBlock, FSContainer)
);
