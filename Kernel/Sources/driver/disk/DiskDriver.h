//
//  DiskDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskDriver_h
#define DiskDriver_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <System/Disk.h>


// A disk driver manages the data stored on a disk. It provides read and write
// access to the disk data. Data on a disk is organized in blocks. All blocks
// are of the same size. Blocks are addresses with an index in the range
// [0, BlockCount]. Note that a disk driver always implements the asynchronous
// driver model.
open_class(DiskDriver, Driver,
);
open_class_funcs(DiskDriver, Driver,

    // Returns information about the disk drive and the media loaded into the
    // drive.
    errno_t (*getInfoAsync)(void* _Nonnull self, DiskInfo* pOutInfo);

    // Reads the contents of the block at index 'lba'. 'buffer' must be big
    // enough to hold the data of a block. Blocks the caller until the read
    // operation has completed. Note that this function will never return a
    // partially read block. Either it succeeds and the full block data is
    // returned, or it fails and no block data is returned.
    // The abstract implementation returns EIO.
    errno_t (*getBlockAsync)(void* _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba);

    // Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
    // must be big enough to hold a full block. Blocks the caller until the
    // write has completed. The contents of the block on disk is left in an
    // indeterminate state of the write fails in the middle of the write. The
    // block may contain a mix of old and new data.
    // The abstract implementation returns EIO.
    errno_t (*putBlockAsync)(void* _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba);
);


//
// Methods for use by disk driver users.
//

extern errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo);
extern errno_t DiskDriver_GetBlock(DiskDriverRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba);
extern errno_t DiskDriver_PutBlock(DiskDriverRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba);

#define DiskDriver_Create(__className, __pOutDriver) \
    _Driver_Create(&k##__className##Class, kDriverModel_Async, (DriverRef*)__pOutDriver)

#endif /* DiskDriver_h */
