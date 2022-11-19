//
//  FloppyDisk.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDisk_h
#define FloppyDisk_h

#include "Foundation.h"
#include "InterruptController.h"
#include "Semaphore.h"


// See http://lclevy.free.fr/adflib/adf_info.html
#define ADF_SECTOR_SIZE         512

#define ADF_DD_SECS_PER_CYL     11
#define ADF_DD_CYLS_PER_DISK    80
#define ADF_DD_HEADS_PER_DISK   2

#define ADF_HD_SECS_PER_CYL     22
#define ADF_HD_CYLS_PER_DISK    80
#define ADF_HD_HEADS_PER_DISK   2


// Sector table capacity
#define FLOPPY_SECTORS_CAPACITY         ADF_HD_SECS_PER_CYL
#define FLOPPY_TRACK_BUFFER_CAPACITY    6400


// Set if the drive's track buffer contains valid data
#define FLOPPY_FLAG_TRACK_BUFFER_VALID  0x01

// Set if the most recent seek step moved inward
#define FLOPPY_FLAG_PREV_STEP_INWARD    0x02


typedef UInt8   FdcControlByte;       // shadow copy of the CIABRPB register


// The floppy DMA singleton. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
typedef struct _FloppyDMA {
    Semaphore           inuse;  // Semaphore indicating whether the DMA is in use
    BinarySemaphore     done;   // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
} FloppyDMA;


// Returns the shared floppy DMA object
extern FloppyDMA* _Nonnull FloppyDMA_GetShared(void);

// Creates the floppy DMA singleton
extern FloppyDMA* _Nullable FloppyDMA_Create(void);



// Stores the state of a single floppy drive.
// !!! Keep in sync with memory.i !!!
typedef struct _FloppyDisk {
    UInt16* _Nonnull    track_buffer;                               // cached track data (MFM encoded)
    Int16               track_size;                                 // cache size in words
    Int16               track_sectors[FLOPPY_SECTORS_CAPACITY];     // table with offsets to the sector starts. The offset points to the first word after the sector sync word(s); 0 means that this sector does not exist
    Int8                head;                                       // currently selected drive head; -1 means unknown -> need to call FloppyDisk_Reset()
    Int8                cylinder;                                   // currently selected drive cylinder; -1 means unknown -> need to call FloppyDisk_Reset()
    Int8                drive;                                      // drive number that this fd object represents
    UInt8               flags;
    FdcControlByte      ciabprb;                                    // shadow copy of the CIA BPRB register for this floppy drive
} FloppyDisk;



extern FloppyDisk* _Nullable FloppyDisk_Create(Int drive);
extern void FloppyDisk_Destroy(FloppyDisk* _Nullable pDisk);

extern void FloppyDisk_Reset(FloppyDisk* _Nonnull pDisk);

extern ErrorCode FloppyDisk_GetStatus(FloppyDisk* _Nonnull pDisk);

extern void FloppyDisk_MotorOn(FloppyDisk* _Nonnull pDisk);
extern void FloppyDisk_MotorOff(FloppyDisk* _Nonnull pDisk);

extern void FloppyDisk_AcknowledgeDiskChange(FloppyDisk* _Nonnull pDisk);

extern ErrorCode FloppyDisk_ReadSector(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder, Int sector, Byte* _Nonnull pBuffer);
extern ErrorCode FloppyDisk_WriteSector(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder, Int sector, const Byte* _Nonnull pBuffer);

#endif /* FloppyDisk_h */
