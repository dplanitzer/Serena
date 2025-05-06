//
//  FloppyDriverPriv.h
//  kernel
//
//  Created by Dietmar Planitzer on 6/17/24.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDriverPriv_h
#define FloppyDriverPriv_h

#include "FloppyDriver.h"
#include "FloppyControllerPkg.h"
#include "adf.h"


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


// Stores the state of a single floppy drive.
final_class_ivars(FloppyDriver, DiskDriver,

    // Buffer used to cache a read track
    ADFSector               sectors[ADF_MAX_SECS_PER_TRACK];    // table of sectorsPerTrack good and bad sectors in the track stored in the track buffer  
    uint16_t* _Nullable     trackBuffer;                        // cached read track data (MFM encoded)
    int16_t                 trackReadWordCount;                 // cached read track buffer size in words
    int16_t                 gapSize;                            // track gap size
    int16_t                 tbCylinder;                         // cylinder of the track stored in the track buffer; -1 if track buffer is empty
    int16_t                 tbHead;                             // head of the track stored in the track buffer; -1 if track buffer is empty

    // Buffer used to compose a track for writing
    uint16_t* _Nullable     trackCompositionBuffer;
    int16_t                 trackWriteWordCount;                // number of words to write to a track
    uint8_t                 sectorDataBuffer[ADF_SECTOR_DATA_SIZE];

    // Disk geometry
    int8_t                  sectorsPerTrack;
    int8_t                  cylindersPerDisk;

    int                     readErrorCount;                     // Number of read errors since last disk driver reset / disk change

    int8_t                  head;                               // currently selected drive head; -1 means unknown -> need to call FloppyDriver_ResetDrive()
    int8_t                  cylinder;                           // currently selected drive cylinder; -1 means unknown -> need to call FloppyDriver_ResetDrive()
    int8_t                  drive;                              // drive number that this fd object represents
    DriveState              driveState;                         // current drive hardware state as maintained by the floppy controller

    struct __fdFlags {
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    motorState:2;
        unsigned int    didResetDrive:1;
        unsigned int    shouldResetDiskChangeStepInward:1;  // tells the reset-disk-change function in which direction to step to trigger a reset of the disk change hardware bit
        unsigned int    isOnline:1;                         // true if a drive is connected
        unsigned int    reserved:26;
    }                       flags;
);


extern errno_t FloppyDriver_Create(DriverRef _Nullable parent, int drive, DriveState ds, FloppyDriverRef _Nullable * _Nonnull pOutDisk);
static void FloppyDriver_EstablishInitialDriveState(FloppyDriverRef _Nonnull self);
static void FloppyDriver_OnMediaChanged(FloppyDriverRef _Nonnull self);
static void FloppyDriver_OnHardwareLost(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_EnsureTrackBuffer(FloppyDriverRef _Nonnull self);
static void FloppyDriver_InvalidateTrackBuffer(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_EnsureTrackCompositionBuffer(FloppyDriverRef _Nonnull self);
static void FloppyDriver_ResetTrackBuffer(FloppyDriverRef _Nonnull self);

static void FloppyDriver_MotorOn(FloppyDriverRef _Nonnull self);
static void FloppyDriver_MotorOff(FloppyDriverRef _Nonnull self);
static errno_t FloppyDriver_WaitForDiskReady(FloppyDriverRef _Nonnull self);
static void FloppyDriver_DelayedMotorOff(FloppyDriverRef _Nonnull self);
static void FloppyDriver_CancelDelayedMotorOff(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_SeekToTrack_0(FloppyDriverRef _Nonnull self, bool* _Nonnull pOutDidStep);
static errno_t FloppyDriver_SeekTo(FloppyDriverRef _Nonnull self, int cylinder, int head);

static void FloppyDriver_ResetDriveDiskChange(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_PrepareIO(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs);
static errno_t FloppyDriver_DoSyncIO(FloppyDriverRef _Nonnull self, bool bWrite);
static errno_t FloppyDriver_FinalizeIO(FloppyDriverRef _Nonnull self, errno_t err);

#define FloppyDriver_TrackFromCylinderAndHead(__chs) (2*(__chs->c) + (__chs->h))

#define FloppyDriver_GetController(__self) \
Driver_GetParentAs(__self, FloppyController)

#endif /* FloppyDriverPriv_h */
