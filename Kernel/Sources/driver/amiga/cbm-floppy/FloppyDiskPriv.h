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


// Sector table capacity
#define ADF_DD_TRACK_WORD_COUNT_TO_READ     6400
#define ADF_DD_TRACK_WORD_COUNT_TO_WRITE    6400


// Stores the state of a single floppy drive.
final_class_ivars(FloppyDisk, DiskDriver,

    FloppyController * _Nonnull fdc;

    // Buffer used to cache a read track (Chip mem)
    uint16_t* _Nonnull          trackBuffer;                        // cached read track data (MFM encoded)
    size_t                      trackBufferSize;                    // cached read track buffer size in words
    ADF_MFMSector * _Nullable   sectors[ADF_HD_SECS_PER_TRACK];     // table with offsets to the sector starts. The offset points to the first word after the sector sync word(s); 0 means that this sector does not exist  
    int                         gapSize;                            // track gap size

    // Disk geometry
    LogicalBlockAddress         logicalBlockCapacity;                   // disk size in terms of logical blocks
    int                         sectorsPerCylinder;
    int                         sectorsPerTrack;
    int                         headsPerCylinder;
    int                         trackWordCountToRead;
    int                         trackWordCountToWrite;

    int                         readErrorCount;                         // Number of read errors since last disk driver reset / disk change

    int8_t                      head;                                   // currently selected drive head; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t                      cylinder;                               // currently selected drive cylinder; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t                      drive;                                  // drive number that this fd object represents
    FdcControlByte              ciabprb;                                // shadow copy of the CIA BPRB register for this floppy drive

    struct __Flags {
        unsigned int    isTrackBufferValid:1;
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    reserved:31;
    }                           flags;
);


static errno_t FloppyDisk_Create(int drive, FloppyController* _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk);
static errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull self);
static void FloppyDisk_DisposeTrackBuffer(FloppyDiskRef _Nonnull self);
static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self);
static errno_t FloppyDisk_WaitDriveReady(FloppyDiskRef _Nonnull self);
static errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull self);
static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull self);
static void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull self);

#endif /* FloppyDiskPriv_h */
