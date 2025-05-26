//
//  DiskDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskDriver_h
#define DiskDriver_h

#include <diskcache/DiskCache.h>
#include <driver/Driver.h>
#include <driver/IORequest.h>
#include <kpi/disk.h>

// Describes the physical properties of the disk that is currently loaded into
// the drive.
typedef struct SensedDisk {
    scnt_t      sectorsPerTrack;
    size_t      heads;
    size_t      cylinders;
    size_t      sectorSize;         // > 0 if a media is loaded; should be the default sector size even if no media is loaded; may be 0
    scnt_t      rwClusterSize;
    scnt_t      frClusterSize;      // > 0 then formatting is supported and a format call takes 'frClusterSize' sectors as input
    uint32_t    properties;         // media properties
} SensedDisk;


enum {
    kDiskRequest_Read = 1,
    kDiskRequest_Write,
    kDiskRequest_Format,
    kDiskRequest_GetInfo,
    kDiskRequest_GetGeometry,
    kDiskRequest_SenseDisk,
};


typedef struct StrategyRequest {
    IORequest       s;
    off_t           offset;         // <- logical sector address in terms of bytes
    unsigned int    options;        // <- read/write options
    ssize_t         resCount;       // -> number of bytes read/written
    size_t          iovCount;       // <- number of I/O vectors in this request

    IOVector        iov[1];
} StrategyRequest;


typedef struct FormatRequest {
    IORequest               s;
    off_t                   offset;     // <- logical sector address in terms of bytes
    const void* _Nonnull    data;       // <- data for all sectors in the cluster to format
    unsigned int            options;    // <- format options
    ssize_t                 resCount;   // -> number of bytes formatted
} FormatRequest;


typedef struct GetDiskInfoRequest {
    IORequest               s;
    diskinfo_t* _Nonnull    ip;
} GetDiskInfoRequest;


typedef struct DiskGeometryRequest {
    IORequest               s;
    diskgeom_t* _Nonnull    gp;
} DiskGeometryRequest;


typedef struct SenseDiskRequest {
    IORequest   s;
} SenseDiskRequest;

    
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
//
// Disk Sensing
//
// A disk driver instance always starts out assuming there is no disk in the
// drive. This is true even for disk drivers that manage a fixed disk. A disk
// driver subclass should kick off the disk sensing from an onStart()
// override and it should eventually call DiskDriver_NoteSensedDisk() with a
// description of the sensed disk.
//
// If a disk is removed from the drive or replaced with a different disk then
// the disk driver should call DiskDriver_NoteDiskChanged() as soon as it detects
// this situation. This will put the disk driver into "disk changed mode". This
// means that all still queued and future disk read/write/format requests will
// fail with a EDISKCHANGED error. The driver remains in disk-changed-mode until
// user space (or kernel space) calls DiskDriver_SenseDisk(). The disk driver
// subclass should check the drive for a disk and determine the disk geometry
// in response to this call and then call DiskDriver_SensedDisk() with the
// information about the sensed disk. This will clear disk-change-mode and queued
// and future disk read/write/format calls will execute on the new disk as
// expected. 
open_class(DiskDriver, Driver,
    DispatchQueueRef _Nullable  dispatchQueue;
    scnt_t                      sectorsPerTrack;
    size_t                      headsPerCylinder;
    size_t                      cylindersPerDisk;
    size_t                      sectorsPerCylinder;
    scnt_t                      sectorCount;        // Number of sectors per media. Is blockCount * s2bFactor
    size_t                      sectorSize;         // Size of a sector in bytes. Usually power-of-2, but may not be. If not, then one sector maps to one logical block with 0 padding at the end
    scnt_t                      rwClusterSize;
    scnt_t                      frClusterSize;
    uint32_t                    mediaProperties;
    uint32_t                    diskId;
    struct __DiskDriverFlags {
        unsigned int        isChsLinear:1;
        unsigned int        isDiskChangeActive:1;
        unsigned int        hasDisk:1;
        unsigned int        reserved:29;
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


    //
    // The following methods are executed on the dispatch queue.
    //

    // Executes a disk request.
    // Default Behavior: XXX
    void (*handleRequest)(void* _Nonnull self, IORequest* _Nonnull req);


    // Executes a strategy request. A strategy request is a list of sector
    // requests. This function is expected to call getSector()/putSector() for
    // each sector referenced by the request.
    // Default Behavior: Calls getSector()/putSector()
    void (*strategy)(void* _Nonnull self, StrategyRequest* _Nonnull req);

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
    void (*doFormat)(void* _Nonnull self, FormatRequest* _Nonnull req);

    // Called from doFormat(). Does the actual formatting of a cluster of sectors. 
    errno_t (*formatSectors)(void* _Nonnull self, const chs_t* chs, const void* _Nonnull data, size_t secSize);


    // Returns information about the disk drive and the media loaded into the
    // drive.
    // Default Behavior: returns info for the currently loaded disk
    void (*doGetInfo)(void* _Nonnull self, GetDiskInfoRequest* _Nonnull req);


    // Returns geometry information about the disk that is currently in the
    // drive. Returns ENOMEDIUM if no disk is in the drive
    // Default Behavior: returns geometry info for the currently loaded disk
    void (*doGetGeometry)(void* _Nonnull self, DiskGeometryRequest* _Nonnull req);


    // Overrides should check whether a disk is in the drive and call
    // DiskDriver_NoteSensedDisk() with information about the sensed disk if
    // one is in the drive or with NULL if no disk is in the drive. This will
    // clear the disk change state of the disk driver.
    void (*doSenseDisk)(void* _Nonnull self, SenseDiskRequest* _Nonnull req);
);


//
// Methods for use by disk driver users.
//

#define DiskDriver_BeginIO(__self, __req) \
invoke_n(beginIO, DiskDriver, __self, __req)

#define DiskDriver_DoIO(__self, __req) \
invoke_n(doIO, DiskDriver, __self, __req)


extern errno_t DiskDriver_Format(DiskDriverRef _Nonnull self, IOChannelRef _Nonnull ch, const void* _Nonnull buf, unsigned int options);

extern errno_t DiskDriver_GetInfo(DiskDriverRef _Nonnull self, diskinfo_t* pOutInfo);

extern errno_t DiskDriver_GetGeometry(DiskDriverRef _Nonnull self, diskgeom_t* pOutInfo);

extern errno_t DiskDriver_SenseDisk(DiskDriverRef _Nonnull self);


//
// Subclassers
//

// Returns the driver's serial dispatch queue. Only call this if the driver is
// an asynchronous driver.
#define DiskDriver_GetDispatchQueue(__self) \
    ((DiskDriverRef)__self)->dispatchQueue

#define DiskDriver_CreateDispatchQueue(__self, __pOutQueue) \
invoke_n(createDispatchQueue, DiskDriver, __self, __pOutQueue)


// Must be called by a subclass in its doSenseDisk() override to indicate whether
// a disk is in the drive or no disk is in the drive. Note that a DiskDriver
// assumes that there is no disk loaded into the drive initially. Thus even a
// fixed disk driver must call this function from its doSenseDisk() override to
// indicate that a fixed disk has been "loaded" into the drive. If the user
// removes the disk from the drive then this function must be called with NULL
// as the info argument; otherwise it must be called with a properly filled in
// info record.
extern void DiskDriver_NoteSensedDisk(DiskDriverRef _Nonnull self, const SensedDisk* _Nullable info);

// Informs the DiskDriver base class that the subclass has detected that the
// disk that's loaded into the drive has changed/was removed from the drive.
// Calling this function causes the disk driver to go into 'disk-change-mode'.
// It stays in this mode until SenseDisk() is called.
extern void DiskDriver_NoteDiskChanged(DiskDriverRef _Nonnull self);

// Returns true if a disk is currently loaded in the drive; false otherwise
#define DiskDriver_HasDisk(__self) \
((((DiskDriverRef)__self)->flags.hasDisk) ? true : false)

// Returns true if a disk change is pending and the disk driver base class expects
// that the subclass calls DiskDriver_NoteSensedDisk() unconditionally even if the
// subclass thinks that the disk loaded state hasn't changed. Eg the subclass thinks
// a disk is loaded and the hardware tells it that a disk is still loaded. This just
// means in this case that the subclass missed the removal of the first disk and
// the insertion of the second disk.
#define DiskDriver_IsDiskChangePending(__self) \
((((DiskDriverRef)__self)->flags.isDiskChangeActive) ? true : false)


#define DiskDriver_HandleRequest(__self, __req) \
invoke_n(handleRequest, DiskDriver, __self, __req)


#define DiskDriver_Strategy(__self, __req) \
invoke_n(strategy, DiskDriver, __self, __req)

#define DiskDriver_GetSector(__self, __chs, __data, __secSize) \
invoke_n(getSector, DiskDriver, __self, __chs, __data, __secSize)

#define DiskDriver_PutSector(__self, __chs, __data, __secSize) \
invoke_n(putSector, DiskDriver, __self, __chs, __data, __secSize)


#define DiskDriver_DoFormat(__self, __req) \
invoke_n(doFormat, DiskDriver, __self, __req)

#define DiskDriver_FormatSectors(__self, __chs, __data, __secSize) \
invoke_n(formatSectors, DiskDriver, __self, __chs, __data, __secSize)


#define DiskDriver_DoGetInfo(__self, __req) \
invoke_n(doGetInfo, DiskDriver, __self, __req)

#define DiskDriver_DoGetGeometry(__self, __req) \
invoke_n(doGetGeometry, DiskDriver, __self, __req)

#define DiskDriver_DoSenseDisk(__self, __req) \
invoke_n(doSenseDisk, DiskDriver, __self, __req)


extern void DiskDriver_LsaToChs(DiskDriverRef _Locked _Nonnull self, sno_t lsa, chs_t* _Nonnull chs);
extern sno_t DiskDriver_ChsToLsa(DiskDriverRef _Locked _Nonnull self, const chs_t* _Nonnull chs);


// Creates a disk driver instance. This function should be called from DiskDrive
// subclass constructors.
// 'options' specifies various behaviors of the disk driver. 'parent' is a
// reference to the bus driver that controls the disk driver.
extern errno_t DiskDriver_Create(Class* _Nonnull pClass, DriverOptions options, DriverRef _Nullable parent, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskDriver_h */
