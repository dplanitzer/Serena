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
#include "FloppyController.h"
#include "AmigaDiskFormat.h"
#include <dispatchqueue/DispatchQueue.h>


// Floppy motor state
enum {
    kMotor_Off = 0,             // Motor not spinning at all
    kMotor_SpinningUp = 1,      // Motor turned on recently and spinning up to target speed
    kMotor_AtTargetSpeed = 2    // Motor is at target speed, disk read/write is permissible 
};


enum {
    kSectorFlag_Exists = 1,     // Sector exists in the track
    kSectorFlag_IsValid = 2,    // Sector is valid (has valid sector info and header checksum checks out)
};

typedef struct ADFSector {
    ADF_SectorInfo  info;
    int16_t         offsetToHeader;     // offset to first word past the sector sync words
    uint16_t        flags;
} ADFSector;


#define ADF_MAX_GAP_LENGTH 1660

// Comes out to 13,628 bytes
// AmigaDOS used a 14,716 bytes buffer
#define ADF_TRACK_BUFFER_SIZE(__sectorsPerTrack) ((__sectorsPerTrack) * (ADF_MFM_SYNC_SIZE + ADF_MFM_SECTOR_SIZE) + ADF_MAX_GAP_LENGTH)


// Stores the state of a single floppy drive.
final_class_ivars(FloppyDisk, DiskDriver,

    Lock                        ioLock;                             // XXX tmp concurrency protection. Will be replaced with a dispatch queue once we've finalized the disk driver & disk cache design 
    DispatchQueueRef _Nonnull   dispatchQueue;

    FloppyController * _Nonnull fdc;

    // Flow control
    TimerRef _Nullable          idleWatcher;
    TimerRef _Nonnull           ondiStateChecker;

    // Buffer used to cache a read track
    ADFSector* _Nullable        sectors;                            // table of sectorsPerTrack good and bad sectors in the track stored in the track buffer  
    uint16_t* _Nullable         trackBuffer;                        // cached read track data (MFM encoded)
    int16_t                     trackBufferWordCount;               // cached read track buffer size in words
    int16_t                     gapSize;                            // track gap size

    // Disk geometry
    LogicalBlockCount           logicalBlockCapacity;                   // disk size in terms of logical blocks
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
        unsigned int    isTrackBufferValid:1;
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    motorState:2;
        unsigned int    isOnline:1;                         // true if a drive is connected
        unsigned int    hasDisk:1;                          // true if disk is in drive
        unsigned int    isOndiStateCheckingActive:1;
        unsigned int    shouldResetDiskChangeStepInward:1;  // tells the reset-disk-change function in which direction to step to trigger a reset of the disk change hardware bit
        unsigned int    reserved:22;
    }                           flags;
);


static errno_t FloppyDisk_Create(int drive, FloppyController* _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk);
static void FloppyDisk_ResetDrive(FloppyDiskRef _Nonnull self);
static void FloppyDisk_DisposeTrackBuffer(FloppyDiskRef _Nonnull self);

static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self);
static void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull self);
static errno_t FloppyDisk_WaitForDiskReady(FloppyDiskRef _Nonnull self);

static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull self, bool* _Nonnull pOutDidStep);
static errno_t FloppyDisk_SeekTo(FloppyDiskRef _Nonnull self, int cylinder, int head);

static void FloppyDisk_StartIdleWatcher(FloppyDiskRef _Nonnull self);
static void FloppyDisk_CancelIdleWatcher(FloppyDiskRef _Nonnull self);
static void FloppyDisk_OnIdle(FloppyDiskRef _Nonnull self);

static void FloppyDisk_UpdateHasDiskState(FloppyDiskRef _Nonnull self);
static void FloppyDisk_ResetDriveDiskChange(FloppyDiskRef _Nonnull self);
static void FloppyDisk_ScheduleOndiStateChecker(FloppyDiskRef _Nonnull self);
static void FloppyDisk_CancelOndiStateChecker(FloppyDiskRef _Nonnull self);
static void FloppyDisk_OnOndiStateCheck(FloppyDiskRef _Nonnull self);

#endif /* FloppyDiskPriv_h */
