//
//  FloppyDiskPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/17/24.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDiskPriv_h
#define FloppyDiskPriv_h

#include "FloppyDisk.h"
#include "FloppyControllerPkg.h"
#include "adf.h"
#include <dispatchqueue/DispatchQueue.h>


// Floppy motor state
enum {
    kMotor_Off = 0,             // Motor not spinning at all
    kMotor_SpinningUp = 1,      // Motor turned on recently and spinning up to target speed
    kMotor_AtTargetSpeed = 2    // Motor is at target speed, disk read/write is permissible 
};


typedef struct ADFSector {
    ADF_SectorInfo  info;
    int16_t         offsetToHeader;     // Offset to first word past the sector sync words (ADF_MFMSector); only valid if 'isHeaderValid' is true
    bool            isHeaderValid;      // Sector header checksum is okay and the info word values make sense
    bool            isDataValid;        // Sector data checksum verification was successful; only true if 'isHeaderValid' is true
} ADFSector;


#define ADF_MAX_GAP_LENGTH 1660

// Track size for reading. Big enough to hold all valid sectors in a track plus
// the biggest possible gap size
// Comes out to 13,628 bytes
// AmigaDOS used a 14,716 bytes buffer
#define ADF_TRACK_READ_SIZE(__sectorsPerTrack) ((__sectorsPerTrack) * (ADF_MFM_SYNC_SIZE + ADF_MFM_SECTOR_SIZE) + ADF_MAX_GAP_LENGTH)

// Track size for writing 
#define ADF_TRACK_WRITE_SIZE(__sectorsPerTrack) ((__sectorsPerTrack) * (ADF_MFM_SYNC_SIZE + ADF_MFM_SECTOR_SIZE) + ADF_MFM_SYNC_SIZE)


// Dispatch queue timer tags
#define kDelayedMotorOffTag     ((uintptr_t)0x1000)
#define kUpdateHasDiskStateTag  ((uintptr_t)0x1001)


// Stores the state of a single floppy drive.
final_class_ivars(FloppyDisk, DiskDriver,

    DispatchQueueRef _Nonnull           dispatchQueue;
    FloppyControllerRef _Nonnull _Weak  fdc;

    // Buffer used to cache a read track
    ADFSector                   sectors[ADF_MAX_SECS_PER_TRACK];    // table of sectorsPerTrack good and bad sectors in the track stored in the track buffer  
    uint16_t* _Nullable         trackBuffer;                        // cached read track data (MFM encoded)
    int16_t                     trackReadWordCount;                 // cached read track buffer size in words
    int16_t                     gapSize;                            // track gap size

    // Buffer used to compose a track for writing
    uint16_t* _Nullable         trackCompositionBuffer;
    int16_t                     trackWriteWordCount;                // number of words to write to a track
    uint8_t                     sectorDataBuffer[ADF_SECTOR_DATA_SIZE];

    // Disk geometry
    LogicalBlockCount           blocksPerDisk;                      // disk size in terms of logical blocks
    int8_t                      sectorsPerCylinder;
    int8_t                      sectorsPerTrack;
    int8_t                      headsPerCylinder;
    int8_t                      cylindersPerDisk;

    int                         readErrorCount;                         // Number of read errors since last disk driver reset / disk change

    int8_t                      head;                                   // currently selected drive head; -1 means unknown -> need to call FloppyDisk_ResetDrive()
    int8_t                      cylinder;                               // currently selected drive cylinder; -1 means unknown -> need to call FloppyDisk_ResetDrive()
    int8_t                      drive;                                  // drive number that this fd object represents
    DriveState                  driveState;                             // current drive hardware state as maintained by the floppy controller

    struct __fdFlags {
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    motorState:2;
        unsigned int    shouldResetDiskChangeStepInward:1;  // tells the reset-disk-change function in which direction to step to trigger a reset of the disk change hardware bit
        unsigned int    isOnline:1;                         // true if a drive is connected
        unsigned int    hasDisk:1;                          // true if disk is in drive
        unsigned int    reserved:24;
    }                           flags;
);

// XXX tmp
typedef struct DiskRequest {
    FloppyDiskRef _Nonnull  self;       // in
    void* _Nonnull          pBuffer;    // in
    LogicalBlockAddress     lba;        // in

    errno_t                 err;        // out
} DiskRequest;
// XXX tmp


extern errno_t FloppyDisk_Create(int drive, DriveState ds, FloppyControllerRef _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk);
static void FloppyDisk_EstablishInitialDriveState(FloppyDiskRef _Nonnull self);
static void FloppyDisk_OnDiskRemoved(FloppyDiskRef _Nonnull self);
static void FloppyDisk_OnHardwareLost(FloppyDiskRef _Nonnull self);

static errno_t FloppyDisk_EnsureTrackBuffer(FloppyDiskRef _Nonnull self);
static void FloppyDisk_DisposeTrackBuffer(FloppyDiskRef _Nonnull self);

static errno_t FloppyDisk_EnsureTrackCompositionBuffer(FloppyDiskRef _Nonnull self);
static void FloppyDisk_DisposeTrackCompositionBuffer(FloppyDiskRef _Nonnull self);

static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self);
static void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull self);
static errno_t FloppyDisk_WaitForDiskReady(FloppyDiskRef _Nonnull self);
static void FloppyDisk_DelayedMotorOff(FloppyDiskRef _Nonnull self);
static void FloppyDisk_CancelDelayedMotorOff(FloppyDiskRef _Nonnull self);

static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull self, bool* _Nonnull pOutDidStep);
static errno_t FloppyDisk_SeekTo(FloppyDiskRef _Nonnull self, int cylinder, int head);

static void FloppyDisk_ScheduleUpdateHasDiskState(FloppyDiskRef _Nonnull self);
static void FloppyDisk_CancelUpdateHasDiskState(FloppyDiskRef _Nonnull self);
static void FloppyDisk_UpdateHasDiskState(FloppyDiskRef _Nonnull self);
static void FloppyDisk_ResetDriveDiskChange(FloppyDiskRef _Nonnull self);

static errno_t FloppyDisk_BeginIO(FloppyDiskRef _Nonnull self, int cylinder, int head);
static errno_t FloppyDisk_DoIO(FloppyDiskRef _Nonnull self, bool bWrite);
static errno_t FloppyDisk_EndIO(FloppyDiskRef _Nonnull self, errno_t err);

#define FloppyDisk_TrackFromCylinderAndHead(__cylinder, __head) (2*__cylinder + __head)

#endif /* FloppyDiskPriv_h */