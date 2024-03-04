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
#include "InterruptController.h"
#include "DiskDriver.h"
#include "Semaphore.h"


// See http://lclevy.free.fr/adflib/adf_info.html
#define ADF_SECTOR_SIZE         512

#define ADF_DD_SECS_PER_TRACK   11
#define ADF_DD_HEADS_PER_CYL    2
#define ADF_DD_CYLS_PER_DISK    80

#define ADF_HD_SECS_PER_TRACK   22
#define ADF_HD_HEADS_PER_CYL    2
#define ADF_HD_CYLS_PER_DISK    80


// Sector table capacity
#define FLOPPY_SECTORS_CAPACITY         ADF_HD_SECS_PER_TRACK
#define FLOPPY_TRACK_BUFFER_CAPACITY    6400


// Set if the drive's track buffer contains valid data
#define FLOPPY_FLAG_TRACK_BUFFER_VALID  0x01

// Set if the most recent seek step moved inward
#define FLOPPY_FLAG_PREV_STEP_INWARD    0x02


typedef uint8_t   FdcControlByte;       // shadow copy of the CIABRPB register


// The floppy DMA singleton. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
typedef struct _FloppyDMA {
    Semaphore           inuse;  // Semaphore indicating whether the DMA is in use
    Semaphore           done;   // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
} FloppyDMA;


extern FloppyDMA* _Nonnull  gFloppyDma;

// Creates the floppy DMA singleton
extern errno_t FloppyDMA_Create(FloppyDMA* _Nullable * _Nonnull pOutFloppyDma);



// Stores the state of a single floppy drive.
// !!! Keep in sync with memory.i !!!
OPEN_CLASS_WITH_REF(FloppyDisk, DiskDriver,
    uint16_t* _Nonnull  track_buffer;                               // cached track data (MFM encoded)
    int16_t             track_size;                                 // cache size in words
    int16_t             track_sectors[FLOPPY_SECTORS_CAPACITY];     // table with offsets to the sector starts. The offset points to the first word after the sector sync word(s); 0 means that this sector does not exist
    int8_t              head;                                       // currently selected drive head; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t              cylinder;                                   // currently selected drive cylinder; -1 means unknown -> need to call FloppyDisk_Reset()
    int8_t              drive;                                      // drive number that this fd object represents
    uint8_t             flags;
    FdcControlByte      ciabprb;                                    // shadow copy of the CIA BPRB register for this floppy drive
);
typedef struct _FloppyDiskMethodTable {
    DiskDriverMethodTable   super;
} FloppyDiskMethodTable;




extern errno_t FloppyDisk_Create(int drive, FloppyDiskRef _Nullable * _Nonnull pOutDisk);

extern errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull pDisk);

extern errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull pDisk);

extern void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull pDisk);
extern void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull pDisk);

extern void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull pDisk);

extern errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, void* _Nonnull pBuffer);
extern errno_t FloppyDisk_WriteSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, const void* _Nonnull pBuffer);

#endif /* FloppyDisk_h */
