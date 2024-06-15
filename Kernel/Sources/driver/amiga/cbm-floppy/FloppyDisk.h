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
#define FLOPPY_SECTORS_CAPACITY         ADF_DD_SECS_PER_TRACK
#define FLOPPY_TRACK_BUFFER_CAPACITY    6400


// Stores the state of a single floppy drive.
open_class_with_ref(FloppyDisk, DiskDriver,
    uint16_t* _Nonnull  writeTrackBuffer;

    uint16_t* _Nonnull  readTrackBuffer;                        // cached read track data (MFM encoded)
    size_t              readTrackBufferSize;                    // cached read track buffer size in words
    size_t              readSectors[FLOPPY_SECTORS_CAPACITY];   // table with offsets to the sector starts. The offset points to the first word after the sector sync word(s); 0 means that this sector does not exist    
    
    int8_t              head;                                   // currently selected drive head; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t              cylinder;                               // currently selected drive cylinder; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t              drive;                                  // drive number that this fd object represents
    FdcControlByte      ciabprb;                                // shadow copy of the CIA BPRB register for this floppy drive
    struct __Flags {
        unsigned int isReadTrackValid:1;
        unsigned int wasMostRecentSeekInward:1;
        unsigned int reserved:31;
    }                   flags;
);
open_class_funcs(FloppyDisk, DiskDriver,
);




extern errno_t FloppyDisk_Create(int drive, FloppyDiskRef _Nullable * _Nonnull pOutDisk);

extern errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull pDisk);

extern errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull pDisk);

extern void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull pDisk);
extern void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull pDisk);

extern void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull pDisk);

extern errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, void* _Nonnull pBuffer);
extern errno_t FloppyDisk_WriteSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, const void* _Nonnull pBuffer);

#endif /* FloppyDisk_h */
