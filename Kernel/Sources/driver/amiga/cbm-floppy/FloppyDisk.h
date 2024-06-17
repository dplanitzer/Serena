//
//  FloppyDisk.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDisk_h
#define FloppyDisk_h

#include <klib/klib.h>
#include <driver/DiskDriver.h>
#include <driver/amiga/cbm-floppy/AmigaDiskFormat.h>    // XXX
#include <driver/amiga/cbm-floppy/FloppyController.h>   // XXX


// Sector table capacity
#define ADF_DD_TRACK_WORD_COUNT_TO_READ     6400
#define ADF_DD_TRACK_WORD_COUNT_TO_WRITE    6400


// Stores the state of a single floppy drive.
open_class_with_ref(FloppyDisk, DiskDriver,
    // Buffer used to cache a read track (Chip mem)
    uint16_t* _Nonnull          trackBuffer;                        // cached read track data (MFM encoded)
    size_t                      trackBufferSize;                    // cached read track buffer size in words
    ADF_MFMSector * _Nullable   sectors[ADF_HD_SECS_PER_TRACK];     // table with offsets to the sector starts. The offset points to the first word after the sector sync word(s); 0 means that this sector does not exist  
    int                         gapSize;                            // track gap size

    // Buffer used to compose a track for writing to disk
    uint16_t* _Nonnull          trackCompositionBuffer;

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
        unsigned int    isReadTrackValid:1;
        unsigned int    wasMostRecentSeekInward:1;
        unsigned int    reserved:31;
    }                           flags;
);
open_class_funcs(FloppyDisk, DiskDriver,
);




extern errno_t FloppyDisk_Create(int drive, FloppyDiskRef _Nullable * _Nonnull pOutDisk);

extern errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull pDisk);

extern errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull pDisk);

extern void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull pDisk);

#endif /* FloppyDisk_h */
