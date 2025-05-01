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
#include <driver/IORequest.h>
#include <System/Disk.h>

// Describes the physical properties of the media that is currently loaded into
// the drive.
typedef struct MediaInfo {
    scnt_t      sectorsPerTrack;
    size_t      heads;
    size_t      cylinders;
    size_t      sectorSize;         // > 0 if a media is loaded; should be the default sector size even if no media is loaded; may be 0
    scnt_t      rwClusterSize;
    scnt_t      frClusterSize;      // > 0 then formatting is supported and a format call takes 'frClusterSize' sectors as input
    uint32_t    properties;         // media properties
} MediaInfo;


enum {
    kDiskRequest_Read = 1,
    kDiskRequest_Write = 2
};


typedef struct DiskRequest {
    IORequest   s;
    off_t       offset;         // <- logical sector address in terms of bytes
    MediaId     mediaId;        // <- disk media identity
    ssize_t     resCount;       // -> number of bytes read/written
    size_t      iovCount;       // <- number of I/O vectors in this request

    IOVector    iov[1];
} DiskRequest;


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
// the interface if you do this.
//
// Note that beginIO() expects that a sector is identified by a byte offset value.
// This byte offset has to be a multiple of the sector size. The length is also
// expressed in bytes but doesn't have to be a multiple of the sector size. If
// it isn't then a read/write is treated as a short read/write that returns the
// number of bytes actually read/written. This number is always a multiple of the
// sector size. 
open_class(DiskDriver, Driver,
    DispatchQueueRef _Nullable  dispatchQueue;
    MediaId                     currentMediaId;
    scnt_t                      sectorsPerTrack;
    size_t                      headsPerCylinder;
    size_t                      cylindersPerDisk;
    size_t                      sectorsPerCylinder;
    scnt_t                      sectorCount;        // Number of sectors per media. Is blockCount * s2bFactor
    size_t                      sectorSize;         // Size of a sector in bytes. Usually power-of-2, but may not be. If not, then one sector maps to one logical block with 0 padding at the end
    scnt_t                      rwClusterSize;
    scnt_t                      frClusterSize;
    uint32_t                    mediaProperties;
    struct __DiskDriverFlags {
        unsigned int        isChsLinear:1;
        unsigned int        reserved:31;
    }                           flags;
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


    //
    // The following methods dispatch to the dispatch queue
    //

    // Starts an asynchronous I/O operation.
    // Override: Optional
    // Default Behavior: Dispatches an async call to the dispatch queue
    errno_t (*beginIO)(void* _Nonnull self, IORequest* _Nonnull req);


    // Starts a asynchronous I/O operation and waits for its completion.
    // Override: Optional
    // Default Behavior: Dispatches a sync call to the dispatch queue
    errno_t (*doIO)(void* _Nonnull self, IORequest* _Nonnull req);


    // Formats 'frClusterSize' consecutive sectors starting at sector 'addr'.
    // 'data' must point to a memory block of size frClusterSize * sectorSize
    // bytes. 'addr' must be a multiple of frClusterSize'. The caller will be
    // blocked until all data has been written to disk or an error is encountered.
    // Override: Optional
    // Default Behavior: Dispatches an async call to doFormat()
    errno_t (*format)(void* _Nonnull self, FormatSectorsRequest* _Nonnull req);


    //
    // The following methods are executed on the dispatch queue.
    //

    // Executes a disk request.
    // Default Behavior: XXX
    void (*strategy)(void* _Nonnull self, IORequest* _Nonnull req);


    // Executes a disk request. A disk request is a list of sector requests. This
    // function is expected to call getSector()/putSector() for each sector
    // request in the disk request and to mark each sector request as done by
    // calling DiskRequest_Done() with the sector request and the final status
    // for the sector request. Note that this function should not call
    // DiskRequest_Done() for the disk request itself.
    // Default Behavior: Calls getSector()/putSector()
    void (*sectorStrategy)(void* _Nonnull self, DiskRequest* _Nonnull req);

    // Reads the contents of the sector at the disk address 'chs' into the
    // in-memory area 'data' of size 'sectorSize'. Blocks the caller until the
    // read operation has completed. Note that this function will never return a
    // partially read sector. Either it succeeds and the full sector data is
    // returned, or it fails and no sector data is returned.
    // The sector address 'chs' is guaranteed to be a valid address.
    // 'secSize' is the sector size in bytes.
    // Default Behavior: returns EIO
    errno_t (*getSector)(void* _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize);

    // Writes the contents of 'data' to the sector 'chs'. Blocks the caller
    // until the write has completed. The contents of the sector on disk is left
    // in an indeterminate state if the write fails in the middle of the write.
    // The sector may contain a mix of old and new data in this case.
    // The sector address 'chs' is guaranteed to be valid.
    // 'secSize' is the sector size in bytes.
    // The abstract implementation returns EIO.
    // Default Behavior: returns EIO
    errno_t (*putSector)(void* _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t secSize);


    // Executes the format action on the dispatch queue. See format() above.
    errno_t (*doFormat)(void* _Nonnull self, FormatSectorsRequest* _Nonnull req);

    // Called from doFormat(). Does the actual formatting of a cluster of sectors. 
    errno_t (*formatSectors)(void* _Nonnull self, const chs_t* chs, const void* _Nonnull data, size_t secSize);
);


//
// Methods for use by disk driver users.
//

extern errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, DiskInfo* pOutInfo);


#define DiskDriver_BeginIO(__self, __req) \
invoke_n(beginIO, DiskDriver, __self, __req)

#define DiskDriver_DoIO(__self, __req) \
invoke_n(doIO, DiskDriver, __self, __req)

extern errno_t DiskDriver_Format(DiskDriverRef _Nonnull self, FormatSectorsRequest* _Nonnull req);


//
// Subclassers
//

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define DiskDriver_GetDispatchQueue(__self) \
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


#define DiskDriver_Strategy(__self, __req) \
invoke_n(strategy, DiskDriver, __self, __req)


#define DiskDriver_SectorStrategy(__self, __req) \
invoke_n(sectorStrategy, DiskDriver, __self, __req)


#define DiskDriver_GetSector(__self, __chs, __data, __secSize) \
invoke_n(getSector, DiskDriver, __self, __chs, __data, __secSize)

#define DiskDriver_PutSector(__self, __chs, __data, __secSize) \
invoke_n(putSector, DiskDriver, __self, __chs, __data, __secSize)


#define DiskDriver_DoFormat(__self, __req) \
invoke_n(doFormat, DiskDriver, __self, __req)

#define DiskDriver_FormatSectors(__self, __chs, __data, __secSize) \
invoke_n(formatSectors, DiskDriver, __self, __chs, __data, __secSize)


extern void DiskDriver_LsaToChs(DiskDriverRef _Locked _Nonnull self, sno_t lsa, chs_t* _Nonnull chs);
extern sno_t DiskDriver_ChsToLsa(DiskDriverRef _Locked _Nonnull self, const chs_t* _Nonnull chs);


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
