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
#include <disk/DiskBlock.h>
#include <driver/Driver.h>
#include <System/Disk.h>


// A disk driver manages the data stored on a disk. It provides read and write
// access to the disk data. Data on a disk is organized in blocks. All blocks
// are of the same size. Blocks are addressed with an index in the range
// [0, BlockCount). Note that a disk driver always implements the asynchronous
// driver model.
//
// A disk driver may either directly control a physical mass storage device or
// it may implement a logical mass storage device which delegates the actual data
// storage to another disk driver. To support the later use case, the DiskDriver
// class supports disk block address virtualization.
//
// The beginIO() method receives the cached disk block that should be read or
// written plus a separate target address that indicates the physical disk block
// that should be read or written. A logical disk driver may map the target
// address that it has received to a new address suitable for another disk driver
// and it then invokes the beginIO() method with the new target address on this
// other driver.
open_class(DiskDriver, Driver,
    DiskId      diskId;
    MediaId     nextMediaId;
);
open_class_funcs(DiskDriver, Driver,

    // Returns information about the disk drive and the media loaded into the
    // drive.
    // Default Behavior: returns info for an empty disk
    errno_t (*getInfo_async)(void* _Nonnull self, DiskInfo* pOutInfo);

    // Returns the media ID associated with the currently inserted disk.
    // kMedia_None is returned if no media is inserted.
    // Default behavior: returns kMedia_None
    MediaId (*getCurrentMediaId)(void* _Nonnull self);

    // Starts an I/O operation on the given block and disk block address
    // 'targetAddr'. Calls getBlock() if the block should be read and putBlock()
    // if it should be written. It then invokes endIO() to notify the system
    // about the completed I/O operation. This function assumes that getBlock()/
    // putBlock() will only return once the I/O operation is done or an error
    // has been encountered.
    // Default Behavior: Calls getBlock/putBlock
    void (*beginIO_async)(void* _Nonnull self, DiskBlockRef _Nonnull pBlock);

    // Reads the contents of the bloc at the disk address 'targetAddr' into the
    // in-memory block 'pBlock'. Blocks the caller until the read operation has
    // completed. Note that this function will never return a partially read
    // block. Either it succeeds and the full block data is returned, or it
    // fails and no block data is returned.
    // Default Behavior: returns EIO
    errno_t (*getBlock)(void* _Nonnull self, DiskBlockRef _Nonnull pBlock);

    // Writes the contents of 'pBlock' to the disk block 'targetAddr'. Blocks
    // the caller until the write has completed. The contents of the block on
    // disk is left in an indeterminate state of the write fails in the middle
    // of the write. The block may contain a mix of old and new data.
    // The abstract implementation returns EIO.
    // Default Behavior: returns EIO
    errno_t (*putBlock)(void* _Nonnull self, DiskBlockRef _Nonnull pBlock);

    // Notifies the system that the I/O operation on the given block has finished
    // and that all data has been read in and stored in the block (if reading) or
    // committed to disk (if writing).
    // Default Behavior: Notifies the disk cache
    void (*endIO)(void* _Nonnull self, DiskBlockRef _Nonnull pBlock, errno_t status);
);


//
// Methods for use by disk driver users.
//

extern errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo);

extern errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, DiskBlockRef _Nonnull pBlock);

//
// Subclassers
//

// The globally unique, non-persistent disk ID of this disk driver. Note that
// this ID is only valid between the end of start() and the beginning of stop().
#define DiskDriver_GetDiskId(__self) \
(((DiskDriverRef)__self)->diskId)

// Generates a new unique media ID. Call this function to generate a new media
// ID after a media change has been detected and use the returned value as the
// new current media ID.
// Note: must be called from the disk driver dispatch queue.
extern MediaId DiskDriver_GetNewMediaId(DiskDriverRef _Nonnull self);

// The ID of the current media in the disk driver. kMediaId_None means that no
// disk is in the drive. This ID is only good enough to detect whether the media
// has changed or is still the same. It is not good enough to identify a
// particular media after it has been removed from the drive and re-inserted.
#define DiskDriver_GetCurrentMediaId(__self) \
invoke_0(getCurrentMediaId, DiskDriver, __self)

#define DiskDriver_GetBlock(__self, __pBlock) \
invoke_n(getBlock, DiskDriver, __self, __pBlock)

#define DiskDriver_PutBlock(__self, __pBlock) \
invoke_n(putBlock, DiskDriver, __self, __pBlock)

#define DiskDriver_EndIO(__self, __pBlock, __status) \
invoke_n(endIO, DiskDriver, __self, __pBlock, __status)

#define DiskDriver_Create(__className, __pOutDriver) \
    _Driver_Create(&k##__className##Class, kDriverModel_Async, kDriver_Exclusive | kDriver_Seekable, (DriverRef*)__pOutDriver)

#endif /* DiskDriver_h */
