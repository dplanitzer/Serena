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

enum {
    kSectorState_Ok =  0,
    kSectorState_Missing,
    kSectorState_BadDataChecksum,
    kSectorState_NotUnique,
};


// DMA track size (R/W): 1660 + 11 * 1088 -> 13,628 bytes
// AmigaDOS used a 14,716 bytes buffer (1660 + 12 * 1088 since it didn't use
// hardware sync).
#define DMA_BYTE_SIZE(__sectorsPerTrack) (ADF_GAP_SIZE + (__sectorsPerTrack) * (ADF_MFM_SYNC_SIZE + ADF_MFM_SECTOR_SIZE))

// Size of the track buffer in bytes
#define TRACK_BUFFER_BYTE_SIZE(__sectorsPerTrack) (sizeof(ADF_Sector) * (__sectorsPerTrack))

// Dispatch queue timer tags
#define kDelayedMotorOffTag     ((uintptr_t)0x1000)
#define kDiskChangeCheckTag     ((uintptr_t)0x1001)


// Stores the state of a single floppy drive.
final_class_ivars(FloppyDriver, DiskDriver,

    // DMA buffer
    uint16_t* _Nonnull      dmaBuffer;
    int16_t                 dmaReadWordCount;
    int16_t                 dmaWriteWordCount;

    // Track buffer
    ADF_Sector* _Nonnull    trackBuffer;
    int8_t                  tbSectorState[ADF_MAX_SECS_PER_TRACK];
    int16_t                 tbTrackNo;

    // Disk geometry
    const DriveParams* _Nonnull params;
    int8_t                      sectorsPerTrack;

    int                     readErrorCount;                     // Number of read errors since last disk driver reset / disk change

    int8_t                  head;                               // currently selected drive head; -1 means unknown -> need to call FloppyDriver_ResetDrive()
    int8_t                  cylinder;                           // currently selected drive cylinder; -1 means unknown -> need to call FloppyDriver_ResetDrive()
    int8_t                  drive;                              // drive number that this fd object represents
    DriveState              driveState;                         // current drive hardware state as maintained by the floppy controller

    struct __fdFlags {
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    motorState:2;
        unsigned int    shouldResetDiskChangeStepInward:1;  // tells the reset-disk-change function in which direction to step to trigger a reset of the disk change hardware bit
        unsigned int    isOnline:1;                         // true if a drive is connected
        unsigned int    dkCount:4;
        unsigned int    dkCountMax:4;
        unsigned int    reserved:19;
    }                       flags;
);


extern errno_t FloppyDriver_Create(DriverRef _Nullable parent, int drive, DriveState ds, const DriveParams* _Nonnull params, FloppyDriverRef _Nullable * _Nonnull pOutDisk);
static void FloppyDriver_EstablishInitialDriveState(FloppyDriverRef _Nonnull self);
static void FloppyDriver_OnMediaChanged(FloppyDriverRef _Nonnull self);
static void FloppyDriver_OnHardwareLost(FloppyDriverRef _Nonnull self);

static void FloppyDriver_MotorOn(FloppyDriverRef _Nonnull self);
static void FloppyDriver_MotorOff(FloppyDriverRef _Nonnull self);
static errno_t FloppyDriver_WaitForDiskReady(FloppyDriverRef _Nonnull self);
static void FloppyDriver_DelayedMotorOff(FloppyDriverRef _Nonnull self);
static void FloppyDriver_CancelDelayedMotorOff(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_SeekToTrack_0(FloppyDriverRef _Nonnull self);
static void FloppyDriver_SeekTo(FloppyDriverRef _Nonnull self, int cylinder, int head);

static void FloppyDriver_ResetDriveDiskChange(FloppyDriverRef _Nonnull self);
static void FloppyDriver_CheckDiskChange(FloppyDriverRef _Nonnull self);
static void FloppyDriver_SetDiskChangeCounter(FloppyDriverRef _Nonnull self);

static errno_t FloppyDriver_PrepareIO(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs);
static errno_t FloppyDriver_DoSyncIO(FloppyDriverRef _Nonnull self, bool bWrite);
static errno_t FloppyDriver_FinalizeIO(FloppyDriverRef _Nonnull self, errno_t err);

#define FloppyDriver_TrackFromCylinderAndHead(__chs) (2*(__chs->c) + (__chs->h))

#define FloppyDriver_GetController(__self) \
Driver_GetParentAs(__self, FloppyController)


#endif /* FloppyDriverPriv_h */
