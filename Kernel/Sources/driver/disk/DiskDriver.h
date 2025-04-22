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
#include <diskcache/DiskCache.h>
#include <driver/Driver.h>
#include <driver/disk/DiskRequest.h>
#include <System/Disk.h>

// Describes the physical properties of the media that is currently loaded into
// the drive.
typedef struct MediaInfo {
    LogicalBlockCount   sectorCount;    // > 0 if a media is loaded; 0 otherwise
    size_t              sectorSize;     // > 0 if a media is loaded; should be the default sector size even if no media is loaded; may be 0 
    uint32_t            properties;     // media properties
} MediaInfo;


// Contextual information passed to doIO().
typedef struct DiskContext {
    MediaId             mediaId;        // ID of currently loaded disk media
    LogicalBlockCount   blockCount;     // Number of logical blocks on the disk
    size_t              blockSize;      // Logical block size in bytes
    size_t              sectorSize;     // Sector size in bytes
    size_t              s2bFactor;      // Sectors per logical block
} DiskContext;


typedef struct brng {
    LogicalBlockAddress lba;
    LogicalBlockCount   count;
} brng_t;


// A disk driver manages the data stored on a disk. It provides read and write
// access to the disk data. Data on a disk is organized in sectors. All sectors
// are of the same size. Sectors are addressed with an index in the range
// [0, SectorCount). Note that a disk driver always implements the asynchronous
// and dispatch queue-based driver model.
//
// A disk driver may either directly control a physical mass storage device or
// it may implement a logical mass storage device which delegates the actual data
// storage to another disk driver.
//
// By default a disk driver operates asynchronously and executes all its I/O
// operations on a (serial) dispatch queue. However a subclass may override the
// createDispatchQueue() method and return NULL to force a synchronous model.
// Note that you should override all methods that are marked as dispatching in
// the interface if you do this. The overrides should then execute the I/O
// operations directly and 
//
//
// Logical block sizes vs sector sizes:
//
// Every disk driver defines a 'sector size' and 'sector count'. These are the
// size of a single physical block or sector on the media and the overall number
// of addressable sectors on the media. These numbers may change when the
// currently loaded media is ejected and replaced with a different media.
//
// Every disk driver is also associated with a disk cache. The disk cache defines
// a 'logical block size'. The logical block size is always a power-of-2 while
// the sector size is usually a power-of-2 but isn't required to be one.
// Eg CD-ROM Audio disks define a sector size of 2,352 bytes.
//
// A disk driver is required to call DiskDriver_NoteMediaLoaded() when a new
// media is loaded into the drive. This function takes a media info that
// describes the sector size and sector count. The disk driver class then
// derives the logical block count and the conversion factor from sector
// size to logical block size. Note that the conversion is defined like this:
// * Power-of-2 sector size: as many sectors as possible are mapped to a
//                           single logical block.
// * Non-power-of-2 sector size: a single sector is mapped to a single logical
//                               block. The remaining bytes are zero-filled on
//                               read and ignored on write.
//
// The disk driver implementation requires that:
// * Logical block size is always a power-of-2
// * Sector size may be a power-of-2
// * Logical block size is always >= sector size
// * if sector size is not a power-of-2, then 1 logical block corresponds
//   to 1 sector
// 
// User space applications and kernel code which wants to invoke the raw read/
// write functions or access a disk through a FSContainer works with the logical
// block size. The sector size is only relevant to the internals of the disk
// driver. That said, the sector size and count are still made available as part
// of the DiskInfo object to enable user space code to show those values to the
// user if desired.
open_class(DiskDriver, Driver,
    DispatchQueueRef _Nullable  dispatchQueue;
    DiskCacheRef _Weak _Nonnull diskCache;      // This is a weak ref because the disk cache holds a strong ref to the driver and the disk cache has to outlive the driver by design (has to be a global object)
    DiskCacheClient             dcClient;       // Protected by disk cache lock
    MediaId                     currentMediaId;
    LogicalBlockCount           blockCount;         // Number of logical blocks per media
    size_t                      blockSize;          // Size of a logical block i nbytes. Always a power-of-2
    uint16_t                    blockShift;         // log2(blockSize)
    uint16_t                    s2bFactor;          // Number of sectors per logical block
    LogicalBlockCount           sectorCount;        // Number of sectors per media. Is blockCount * s2bFactor
    size_t                      sectorSize;         // Size of a sector in bytes. Usually power-of-2, but may not be. If not, then one sector maps to one logical block with 0 padding at the end 
    uint32_t                    mediaProperties;
);
open_class_funcs(DiskDriver, Driver,

    // Creates a dispatch queue for the driver. The default implementation creates
    // a serial queue suitable for a disk driver. A subclass may override this
    // method and return NULL to disable queuing. Note that if you do this in a
    // subclass that you should also override all relevant methods below to
    // execute the action directly instead of dispatching to a queue that doesn't
    // exist. 
    // Override: Optional
    // Default Behavior: create a dispatch queue with priority Normal
    errno_t (*createDispatchQueue)(void* _Nonnull self, DispatchQueueRef _Nullable * _Nonnull pOutQueue);


    // Returns information about the disk drive and the media loaded into the
    // drive.
    // Default Behavior: returns info for an empty disk
    void (*getInfo)(void* _Nonnull _Locked self, DiskInfo* _Nonnull pOutInfo);


    // XXX Experimental
    // Returns the range of consecutive blocks that should be fetched from disk
    // or written to disk in a single disk request. This allows a disk driver
    // subclass to optimize reading/writing disks in the sense that a whole
    // track worth of data can be processed in a single disk request.
    // Default behavior: returns a block range of size 1 and the provided lba
    errno_t (*getRequestRange2)(void* _Nonnull self, MediaId mediaId, LogicalBlockAddress lba, brng_t* _Nonnull pOutBlockRange);


    //
    // The following methods dispatch to the dispatch queue
    //

    // Starts an I/O operation for the given disk request. Dispatches an async
    // call to doIO() on the dispatch queue. The actual read/write will happen
    // asynchronously.
    // Default Behavior: Dispatches an async call to doIO()
    errno_t (*beginIO)(void* _Nonnull _Locked self, DiskRequest* _Nonnull req);
    

    //
    // The following methods are executed on the dispatch queue.
    //

    // Executes a disk request. A disk request is a list of block requests. This
    // function is expected to call 'doBlockIO' for each block request in
    // the disk request and to mark each block request as done by calling
    // DiskRequest_Done() with the block request and the final status for the
    // block request. Note that this function should not call DiskRequest_Done()
    // for the disk request itself.
    // Default Behavior: Calls doBlockIO()
    void (*doIO)(void* _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req);

    // Executes a block request. This function is expected to validate the block
    // request parameters based on the provided disk context 'ctx'. It should
    // then read/write the block data while taking care of the translation from
    // a logical block size to the sector size.
    // Default Behavior: Calls getSector/putSector
    errno_t (*doBlockIO)(void* _Nonnull self, const DiskContext* _Nonnull ctx, DiskRequest* _Nonnull req, BlockRequest* _br);

    // Reads the contents of the physical block at the disk address 'ba'
    // into the in-memory area 'data' of size 'blockSize'. Blocks the caller
    // until the read operation has completed. Note that this function will
    // never return a partially read block. Either it succeeds and the full
    // block data is returned, or it fails and no block data is returned.
    // The sector address 'ba' is guaranteed to be in the range
    // [0, sectorCount).
    // 'mbSize' is the sector size in bytes.
    // Default Behavior: returns EIO
    errno_t (*getSector)(void* _Nonnull self, LogicalBlockAddress ba, uint8_t* _Nonnull data, size_t secSize);

    // Writes the contents of 'data' to the physical block 'ba'. Blocks
    // the caller until the write has completed. The contents of the block on
    // disk is left in an indeterminate state of the write fails in the middle
    // of the write. The block may contain a mix of old and new data.
    // The sector address 'ba' is guaranteed to be in the range
    // [0, sectorCount).
    // 'mbSize' is the sector size in bytes.
    // The abstract implementation returns EIO.
    // Default Behavior: returns EIO
    errno_t (*putSector)(void* _Nonnull self, LogicalBlockAddress ba, const uint8_t* _Nonnull data, size_t secSize);
);


//
// Methods for use by disk driver users.
//

// Returns a reference to the disk cache that caches disk data on behalf of the
// receiver. Clients that want to read/write data from/to this disk driver should
// go through the disk cache APIs and use the block size defined by this disk
// cache rather than what the driver GetInfo() call returns. Note that a disk
// driver is always associated with a disk cache.
#define DiskDriver_GetDiskCache(__self) \
((DiskDriverRef)__self)->diskCache

extern errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo);


extern errno_t DiskDriver_GetRequestRange(DiskDriverRef _Nonnull self, MediaId mediaId, LogicalBlockAddress lba, brng_t* _Nonnull pOutBlockRange);

#define DiskDriver_GetRequestRange2(__self, __mediaId, __lba, __brng) \
invoke_n(getRequestRange2, DiskDriver, __self, __mediaId, __lba, __brng)


extern errno_t DiskDriver_BeginIO(DiskDriverRef _Nonnull self, DiskRequest* _Nonnull req);


//
// Methods for use by the disk cache
//
#define DiskDriver_GetDiskCacheClient(__self) \
&((DiskDriverRef)__self)->dcClient


//
// Subclassers
//

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define Driver_GetDispatchQueue(__self) \
    ((DiskDriverRef)__self)->dispatchQueue

#define DiskDriver_CreateDispatchQueue(__self, __pOutQueue) \
invoke_n(createDispatchQueue, DiskDriver, __self, __pOutQueue)


// Must be called by a subclass whenever a new media is loaded into the driver
// or removed from the drive. Note that DiskDriver assumes that there is no
// media loaded into the drive, initially. Thus even a fixed disk driver must
// call this function at creation/start time to indicate that a fixed media has
// been "loaded" into the drive. If the user removes the disk from the drive
// then this function must be called with NULL as the info argument; otherwise
// it must be called with a properly filled in media info record.
// This function generates the required unique media id. 
extern void DiskDriver_NoteMediaLoaded(DiskDriverRef _Nonnull self, const MediaInfo* _Nullable info);


#define DiskDriver_DoIO(__self, __req, __ctx) \
invoke_n(doIO, DiskDriver, __self, __req, __ctx)

#define DiskDriver_DoBlockIO(__self, __req, __br, __ctx) \
invoke_n(doBlockIO, DiskDriver, __self, __req, __br, __ctx)


#define DiskDriver_GetSector(__self, __ba, __data, __secSize) \
invoke_n(getSector, DiskDriver, __self, __ba, __data, __secSize)

#define DiskDriver_PutSector(__self, __ba, __data, __secSize) \
invoke_n(putSector, DiskDriver, __self, __ba, __data, __secSize)


// Creates a disk driver instance. This function should be called from DiskDrive
// subclass constructors.
// 'options' specifies various behaviors of the disk driver. 'parent' is a
// reference to the bus driver that controls the disk driver. 'info' is a
// description of the media loaded into the drive when the driver is instantiated.
// It may be that the media is not really known at that point in time. In this
// case either pass NULL or (preferably) a media info with blockCount == 0 and
// the other properties set to defaults that make sense for the drive in general.
extern errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, const MediaInfo* _Nullable info, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskDriver_h */
