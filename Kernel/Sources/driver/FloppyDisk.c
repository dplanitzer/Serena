//
//  FloppyDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDisk.h"
#include <dispatcher/VirtualProcessor.h>
#include <hal/Platform.h>


// CIABPRA bits (FDC status byte)
#define CIABPRA_BIT_DSKRDY      5
#define CIABPRA_BIT_DSKTRACK0   4
#define CIABPRA_BIT_DSKPROT     3
#define CIABPRA_BIT_DSKCHANGE   2
#define CIABPRA_BIT_IODONE      0   /* see fdc_get_io_status() */


// CIABPRB bits (FDC control byte)
#define CIABPRB_BIT_DSKMOTOR    7
#define CIABPRB_BIT_DSKSEL3     6
#define CIABPRB_BIT_DSKSEL2     5
#define CIABPRB_BIT_DSKSEL1     4
#define CIABPRB_BIT_DSKSEL0     3
#define CIABPRB_BIT_DSKSIDE     2
#define CIABPRB_BIT_DSKDIREC    1
#define CIABPRB_BIT_DSKSTEP     0



extern unsigned int fdc_get_drive_status(FdcControlByte* _Nonnull fdc);
extern void fdc_set_drive_motor(FdcControlByte* _Nonnull fdc, int onoff);
extern void fdc_step_head(FdcControlByte* _Nonnull fdc, int inout);
extern void fdc_select_head(FdcControlByte* _Nonnull fdc, int side);
extern void fdc_io_begin(FdcControlByte* _Nonnull fdc, uint16_t* pData, int nwords, int readwrite);
extern unsigned int fdc_get_io_status(FdcControlByte* _Nonnull fdc);
extern void fdc_io_end(FdcControlByte*  _Nonnull fdc);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Floppy DMA
////////////////////////////////////////////////////////////////////////////////


FloppyDMA*  gFloppyDma;

// Destroys the floppy DMA.
static void FloppyDMA_Destroy(FloppyDMA* _Nullable pDma)
{
    if (pDma) {
        if (pDma->irqHandler != 0) {
            try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, pDma->irqHandler));
        }
        pDma->irqHandler = 0;
        
        Semaphore_Deinit(&pDma->inuse);
        Semaphore_Deinit(&pDma->done);
        
        kfree(pDma);
    }
}

// Creates the floppy DMA singleton
errno_t FloppyDMA_Create(FloppyDMA* _Nullable * _Nonnull pOutFloppyDma)
{
    decl_try_err();
    FloppyDMA* pDma;
    
    try(kalloc_cleared(sizeof(FloppyDMA), (void**) &pDma));

    Semaphore_Init(&pDma->inuse, 1);
    Semaphore_Init(&pDma->done, 0);
        
    try(InterruptController_AddSemaphoreInterruptHandler(gInterruptController,
                                                         INTERRUPT_ID_DISK_BLOCK,
                                                         INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                         &pDma->done,
                                                         &pDma->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController,
                                                       pDma->irqHandler,
                                                       true);
    *pOutFloppyDma = pDma;
    return EOK;

catch:
    FloppyDMA_Destroy(pDma);
    *pOutFloppyDma = NULL;
    return err;
}

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
static errno_t FloppyDMA_DoIO(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords, int readwrite)
{
    decl_try_err();
    
    try(Semaphore_Acquire(&pDma->inuse, kTimeInterval_Infinity));
    
    fdc_io_begin(pFdc, pData, nwords, 0);
    err = Semaphore_Acquire(&pDma->done, TimeInterval_MakeSeconds(10));
    if (err == EOK) {
        const unsigned int status = fdc_get_io_status(pFdc);
        
        if ((status & (1 << CIABPRA_BIT_DSKRDY)) != 0) {
            err = ENOMEDIUM;
        }
        else if ((status & (1 << CIABPRA_BIT_DSKCHANGE)) == 0) {
            err = EDISKCHANGE;
        }
        else if ((status & (1 << CIABPRA_BIT_IODONE)) != 0) {
            err = EOK;
        }
    }
    fdc_io_end(pFdc);
    
    Semaphore_Release(&pDma->inuse);
    
    return (err == ETIMEDOUT) ? ENOMEDIUM : err;

catch:
    return err;
}

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
static errno_t FloppyDMA_Read(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords)
{
    return FloppyDMA_DoIO(pDma, pFdc, pData, nwords, 0);
}

// Synchronously writes 'nwords' 16bit words from the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred to
// disk.
static errno_t FloppyDMA_Write(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, const uint16_t* _Nonnull pData, int nwords)
{
    return FloppyDMA_DoIO(pDma, pFdc, (uint16_t*)pData, nwords, 1);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: MFM decoding
////////////////////////////////////////////////////////////////////////////////


#define MFM_SYNC_WORD   0x4489
#define ADF_FORMAT_V1   0xff

typedef struct _ADF_SectorHeader
{
    uint8_t   format;     // Amiga 1.0 format 0xff
    uint8_t   track;
    uint8_t   sector;
    uint8_t   seceow;
    uint32_t  zero;
    uint32_t  header_crc;
    uint32_t  data_crc;
} ADF_SectorHeader;


// The MFM decoder/encoder code is based on:
// see http://lclevy.free.fr/adflib/adf_info.html
//
// The following copyright notice applies to the functions:
// mfm_decode_sector(), mfm_encode_sector()
//
//
// This document is Copyright (C) 1997-1999 by Laurent Clévy, but may be freely distributed, provided the author name and addresses are included and no money is charged for this document.
//
// This document is provided "as is". No warranties are made as to its correctness.
//
// Amiga and AmigaDOS are registered Trademarks of Gateway 2000.
// Macintosh is a registered Trademark of Apple.
//


// MFM decodes a sector.
// \param input    MFM coded data buffer (size == 2*data_size)
// \param output    decoded data buffer (size == data_size)
// \param data_size size in long, 1 for header's info, 4 for header's sector label
static void mfm_decode_sector(const uint32_t* input, uint32_t* output, int data_size)
{
#define MASK 0x55555555    /* 01010101 ... 01010101 */
    uint32_t odd_bits, even_bits;
    uint32_t chksum = 0;
    
    /* the decoding is made here long by long : with data_size/4 iterations */
    for (int count = 0; count < data_size; count++) {
        odd_bits = *input;                  /* longs with odd bits */
        even_bits = *(input + data_size);   /* longs with even bits : located 'data_size' bytes farther */
        chksum ^= odd_bits;
        chksum ^= even_bits;
        /*
         * MFM decoding, explained on one byte here (o and e will produce t) :
         * the MFM bytes 'abcdefgh' == o and 'ijklmnop' == e will become
         * e & 0x55U = '0j0l0n0p'
         * ( o & 0x55U) << 1 = 'b0d0f0h0'
         * '0j0l0n0p' | 'b0d0f0h0' = 'bjdlfnhp' == t
         */
        *output = (even_bits & MASK) | ((odd_bits & MASK) << 1);
        input++;        /* next 'odd' long and 'even bits' long  */
        output++;       /* next location of the future decoded long */
    }
    chksum &= MASK;    /* must be 0 after decoding */
}

// MFM encodes a sector
static void mfm_encode_sector(const uint32_t* input, uint32_t* output, int data_size)
{
    uint32_t* output_odd = output;
    uint32_t* output_even = output + data_size;
    
    for (int count = 0; count < data_size; count++) {
        const uint32_t data = *input;
        uint32_t odd_bits = 0;
        uint32_t even_bits = 0;
        uint32_t prev_odd_bit = 0;
        uint32_t prev_even_bit = 0;
        
        //    user's data bit      MFM coded bits
        //    ---------------      --------------
        //    1                    01
        //    0                    10 if following a 0 data bit
        //    0                    00 if following a 1 data bit
        for (int8_t i_even = 30; i_even >= 0; i_even -= 2) {
            const int8_t i_odd = i_even + 1;
            const uint32_t cur_odd_bit = data & (1 << i_odd);
            const uint32_t cur_even_bit = data & (1 << i_even);
            
            if (cur_odd_bit) {
                odd_bits |= (1 << i_even);
            } else if (!cur_odd_bit && !prev_odd_bit) {
                odd_bits |= (1 << i_odd);
            }
            
            if (cur_even_bit) {
                even_bits |= (1 << i_even);
            } else if (!cur_even_bit && !prev_even_bit) {
                even_bits |= (1 << i_odd);
            }
            
            prev_odd_bit = cur_odd_bit;
            prev_even_bit = cur_even_bit;
        }
        
        *output_odd = odd_bits;
        *output_even = even_bits;
        
        input++;
        output_odd++;
        output_even++;
    }
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: API
////////////////////////////////////////////////////////////////////////////////

static void FloppyDisk_InvalidateTrackBuffer(FloppyDiskRef _Nonnull pDisk);


// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
errno_t FloppyDisk_Create(int drive, FloppyDiskRef _Nullable * _Nonnull pOutDisk)
{
    decl_try_err();
    FloppyDisk* pDisk;
    
    try(Object_Create(FloppyDisk, &pDisk));
    try(kalloc_options(sizeof(uint16_t) * FLOPPY_TRACK_BUFFER_CAPACITY, KALLOC_OPTION_UNIFIED, (void**) &pDisk->track_buffer));
    
    pDisk->track_size = FLOPPY_TRACK_BUFFER_CAPACITY;
    pDisk->head = -1;
    pDisk->cylinder = -1;
    pDisk->drive = drive;
    pDisk->ciabprb = 0xf9;     // motor off; all drives deselected; head 0; stepping off
    pDisk->ciabprb &= ~(1 << ((drive & 0x03) + 3));    // select this drive
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    *pOutDisk = pDisk;
    return EOK;
    
catch:
    Object_Release(pDisk);
    *pOutDisk = NULL;
    return err;
}

static void FloppyDisk_deinit(FloppyDiskRef _Nonnull pDisk)
{
    kfree(pDisk->track_buffer);
    pDisk->track_buffer = NULL;
}

// Invalidates the track cache.
static void FloppyDisk_InvalidateTrackBuffer(FloppyDiskRef _Nonnull pDisk)
{
    if ((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0) {
        pDisk->flags &= ~FLOPPY_FLAG_TRACK_BUFFER_VALID;
        
        for (int i = 0; i < FLOPPY_SECTORS_CAPACITY; i++) {
            pDisk->track_sectors[i] = 0;
        }
    }
}

// Computes and returns the floppy status from the given fdc drive status.
static inline errno_t FloppyDisk_StatusFromDriveStatus(unsigned int drvstat)
{
    if ((drvstat & (1 << CIABPRA_BIT_DSKRDY)) != 0) {
        return ENOMEDIUM;
    }
    if ((drvstat & (1 << CIABPRA_BIT_DSKCHANGE)) == 0) {
        return EDISKCHANGE;
    }
    
    return EOK;
}

// Waits until the drive is ready (motor is spinning at full speed). This function
// waits for at most 500ms for the disk to become ready.
// Returns S_OK if the drive is ready; ETIMEDOUT  if the drive failed to become
// ready in time.
static errno_t FloppyDisk_WaitDriveReady(FloppyDiskRef _Nonnull pDisk)
{
    decl_try_err();

    for (int ntries = 0; ntries < 50; ntries++) {
        const unsigned int status = fdc_get_drive_status(&pDisk->ciabprb);
        
        if ((status & (1 << CIABPRA_BIT_DSKRDY)) == 0) {
            return EOK;
        }
        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(10)));
    }
    
    return ETIMEDOUT;

catch:
    return err;
}

// Seeks to track #0 and selects head #0. Returns EOK if the function sought at
// least once.
// Note that this function is expected to implicitly acknowledge a disk change if
// it has actually sought.
static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull pDisk)
{
    decl_try_err();
    bool did_step_once = false;
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(18)));
    
    for(;;) {
        const unsigned int status = fdc_get_drive_status(&pDisk->ciabprb);
        
        if ((status & (1 << CIABPRA_BIT_DSKTRACK0)) == 0) {
            break;
        }
        
        fdc_step_head(&pDisk->ciabprb, -1);
        did_step_once = true;
        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3)));
    }
    fdc_select_head(&pDisk->ciabprb, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(15)));
    
    pDisk->head = 0;
    pDisk->cylinder = 0;
    pDisk->flags &= ~FLOPPY_FLAG_PREV_STEP_INWARD;
    
    return (did_step_once) ? EOK : EIO;

catch:
    return err;
}

// Seeks to the specified cylinder and selects the specified drive head.
// (0: outermost, 80: innermost, +: inward, -: outward).
// Returns EDISKCHANGE if the disk has changed.
// Note that we purposefully treat a disk change as an error. We don't want to
// implicitly and accidentally acknowledge a disk change as a side effect of seeking.
// The user of the API needs to become aware of the disk change so that he can actually
// handle it in a sensible way.
static errno_t FloppyDisk_SeekTo(FloppyDiskRef _Nonnull pDisk, int cylinder, int head)
{
    decl_try_err();
    const int diff = cylinder - pDisk->cylinder;
    const int cur_dir = (diff >= 0) ? 1 : -1;
    const int last_dir = (pDisk->flags & FLOPPY_FLAG_PREV_STEP_INWARD) ? 1 : -1;
    const int nsteps = __abs(diff);
    const bool change_side = (pDisk->head != head);
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    const int seek_pre_wait_ms = (nsteps > 0 && cur_dir != last_dir) ? 18 : 0;
    const int side_pre_wait_ms = 2;
    const int pre_wait_ms = __max(seek_pre_wait_ms, side_pre_wait_ms);
    
    if (pre_wait_ms > 0) {
        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(pre_wait_ms)));
    }
    
    
    // Seek if necessary
    if (nsteps > 0) {
        for (int i = nsteps; i > 0; i--) {
            try(FloppyDisk_GetStatus(pDisk));
            
            fdc_step_head(&pDisk->ciabprb, cur_dir);
            pDisk->cylinder += cur_dir;
            
            if (cur_dir >= 0) {
                pDisk->flags |= FLOPPY_FLAG_PREV_STEP_INWARD;
            } else {
                pDisk->flags &= ~FLOPPY_FLAG_PREV_STEP_INWARD;
            }
            
            try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3)));
        }
    }
    
    // Switch heads if necessary
    if (change_side) {
        fdc_select_head(&pDisk->ciabprb, head);
        pDisk->head = head;
    }
    
    // Seek settle time: 15ms
    // Head select settle time: 100us
    const int seek_settle_us = (nsteps > 0) ? 15*1000 : 0;
    const int side_settle_us = (change_side) ? 100 : 0;
    const int settle_us = __max(seek_settle_us, side_settle_us);
    
    if (settle_us > 0) {
        try(VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(settle_us)));
    }
    
    return EOK;

catch:
    return err;
}

// Resets the floppy drive. This function figures out whether there is an actual
// physical floppy drive connected and whether it responds to commands and it then
// moves the disk head to track #0.
// Note that this function leaves the floppy motor turned on and that it implicitly
// acknowledges any pending disk change.
// Upper layer code should treat this function like a disk change.
errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull pDisk)
{
    decl_try_err();

    FloppyDisk_InvalidateTrackBuffer(pDisk);
    pDisk->head = -1;
    pDisk->cylinder = -1;
    
    
    // Turn the motor on to see whether there is an actual drive connected
    FloppyDisk_MotorOn(pDisk);
    try(FloppyDisk_GetStatus(pDisk));
    
    
    // Move the head to track #0
    err = FloppyDisk_SeekToTrack_0(pDisk);
    
    
    // We didn't seek if we were already at track #0. So step to track #1 and
    // then back to #0 to acknowledge a disk change.
    if (err != EOK) {
        fdc_step_head(&pDisk->ciabprb,  1);
        fdc_step_head(&pDisk->ciabprb, -1);
    }

    return EOK;

catch:
    return err;
}

// Returns the current floppy drive status.
errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull pDisk)
{
    return FloppyDisk_StatusFromDriveStatus(fdc_get_drive_status(&pDisk->ciabprb));
}

// The following functions may return an EDISKCHANGE error when called:
// - FloppyDisk_GetStatus()
// - FloppyDisk_ReadSector()
// - FloppyDisk_WriteSector()
//
// You MUST either call FloppyDisk_AcknowledgeDiskChange() or FloppyDisk_Reset()
// in this case to acknowledge the disk change. If the FloppyDisk_GetStatus()
// function continues to return EDISKCHANGE after acking' the disk change, then
// you know that there is no disk in the disk drive.
void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull pDisk)
{
    // Step by one track. This clears the disk change drive state if there is a
    // disk in the drive. If the disk change state doesn't change after the seek
    // then this means that there is truly no disk in the drive.
    // Also invalidate the cache 'cause it is certainly no longer valid.
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    const int dir = (pDisk->cylinder == 0) ? 1 : -1;
    fdc_step_head(&pDisk->ciabprb, dir);
}

// Turns the drive motor on and blocks the caller until the disk is ready.
void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull pDisk)
{
    fdc_set_drive_motor(&pDisk->ciabprb, 1);
    
    const errno_t err = FloppyDisk_WaitDriveReady(pDisk);
    if (err == ETIMEDOUT) {
        fdc_set_drive_motor(&pDisk->ciabprb, 0);
        return;
    }
}

// Turns the drive motor off.
void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull pDisk)
{
    fdc_set_drive_motor(&pDisk->ciabprb, 0);
}

static errno_t FloppyDisk_ReadTrack(FloppyDiskRef _Nonnull pDisk, int head, int cylinder)
{
    decl_try_err();
    
    // Seek to the required cylinder and select the required head
    if (pDisk->cylinder != cylinder || pDisk->head != head) {
        try(FloppyDisk_SeekTo(pDisk, cylinder, head));
    }
    
    
    // Nothing to do if we already have this track cached in the track buffer
    if ((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0) {
        return EOK;
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    try(FloppyDisk_GetStatus(pDisk));
    
    
    // Read the track
    try(FloppyDMA_Read(gFloppyDma, &pDisk->ciabprb, pDisk->track_buffer, pDisk->track_size));
    
    
    // Clear out the sector table
    uint16_t* track_buffer = pDisk->track_buffer;
    int16_t* sector_table = pDisk->track_sectors;
    const int16_t track_buffer_size = pDisk->track_size;
    
    for (int i = 0; i < FLOPPY_SECTORS_CAPACITY; i++) {
        sector_table[i] = 0;
    }
    
    
    // Build the sector table
    for (int16_t i = 0; i < track_buffer_size; i++) {
        ADF_SectorHeader header;
        
        // Find the sync words
        while ((i < track_buffer_size) && track_buffer[i] != MFM_SYNC_WORD) {
            i++;
        }
        
        // Skip past the sync words
        while ((i < track_buffer_size) && track_buffer[i] == MFM_SYNC_WORD) {
            i++;
        }
        
        if (i == track_buffer_size) {
            break;
        }
        
        // MFM decode the sector header
        mfm_decode_sector((const uint32_t*)&track_buffer[i], (uint32_t*)&header.format, 1);
        
        // Validate the sector header. We record valid sectors only.
        if (header.format != ADF_FORMAT_V1 || header.track != cylinder || header.sector >= ADF_DD_SECS_PER_TRACK) {
            continue;
        }
        
        // Record the sector. Note that a sector may appear more than once because
        // we may have read more data from the disk than fits in a single track. We
        // keep the first occurrence of a sector.
        if (sector_table[header.sector] == 0) {
            sector_table[header.sector] = i;
        }
    }
    
    pDisk->flags |= FLOPPY_FLAG_TRACK_BUFFER_VALID;
    
    return EOK;

catch:
    return err;
}

errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, void* _Nonnull pBuffer)
{
    decl_try_err();
    
    // Read the track
    try(FloppyDisk_ReadTrack(pDisk, head, cylinder));
    
    
    // Get the sector
    const uint16_t* track_buffer = pDisk->track_buffer;
    const int16_t* sector_table = pDisk->track_sectors;
    const int16_t idx = sector_table[sector];
    if (idx == 0) {
        return EIO;
    }
    
    
    // MFM decode the sector data
    mfm_decode_sector((const uint32_t*)&track_buffer[idx + 28], (uint32_t*)pBuffer, ADF_SECTOR_SIZE / sizeof(uint32_t));
    
    /*
     for(int i = 0; i < ADF_HD_SECS_PER_CYL; i++) {
     int offset = sector_table[i];
     
     if (offset != 0) {
     print("H: %d, C: %d, S: %d -> %d  (0x%x)\n", head, cylinder, i, offset, &track_buffer[offset]);
     }
     }
     */
    return EOK;

catch:
    return err;
}

static errno_t FloppyDisk_WriteTrack(FloppyDiskRef _Nonnull pDisk, int head, int cylinder)
{
    decl_try_err();
    
    // There must be a valid track cache
    assert((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0);
    
    
    // Seek to the required cylinder and select the required head
    if (pDisk->cylinder != cylinder || pDisk->head != head) {
        try(FloppyDisk_SeekTo(pDisk, cylinder, head));
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    try(FloppyDisk_GetStatus(pDisk));
    
    
    // write the track
    try(FloppyDMA_Write(gFloppyDma, &pDisk->ciabprb, pDisk->track_buffer, pDisk->track_size));
    
    return EOK;

catch:
    return err;
}

errno_t FloppyDisk_WriteSector(FloppyDiskRef _Nonnull pDisk, size_t head, size_t cylinder, size_t sector, const void* pBuffer)
{
    decl_try_err();
    
    // Make sure that we have the track in memory
    try(FloppyDisk_ReadTrack(pDisk, head, cylinder));
    
    
    // Override the sector with the new data
    uint16_t* track_buffer = pDisk->track_buffer;
    const int16_t* sector_table = pDisk->track_sectors;
    const int16_t idx = sector_table[sector];
    if (idx == 0) {
        return EIO;
    }
    
    
    // MFM encode the sector data
    mfm_encode_sector((const uint32_t*)pBuffer, (uint32_t*)&track_buffer[idx + 28], ADF_SECTOR_SIZE / sizeof(uint32_t));
    
    
    // Write the track back out
    // XXX should just mark the track buffer as dirty
    // XXX the cache_invalidate() function should then write the cache back to disk before we seek / switch heads
    // XXX there should be a floppy_is_cache_dirty() and floppy_flush_cache() which writes the cache to disk
    try(FloppyDisk_WriteTrack(pDisk, head, cylinder));
    
    return EOK;

catch:
    return err;
}

// Returns the size of a block.
size_t FloppyDisk_getBlockSize(FloppyDiskRef _Nonnull self)
{
    return ADF_SECTOR_SIZE;
}

// Returns the number of blocks that the disk is able to store.
LogicalBlockCount FloppyDisk_getBlockCount(FloppyDiskRef _Nonnull self)
{
    // XXX detect DD vs HD disk types
    return ADF_HD_SECS_PER_TRACK * ADF_HD_CYLS_PER_DISK * ADF_HD_HEADS_PER_CYL;
}

// Returns true if the disk if read-only.
bool FloppyDisk_isReadOnly(FloppyDiskRef _Nonnull self)
{
    return false;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t FloppyDisk_getBlock(FloppyDiskRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    // XXX hardcoded to HD for now
    const size_t c = lba / (ADF_HD_HEADS_PER_CYL * ADF_HD_SECS_PER_TRACK);
    const size_t h = (lba / ADF_HD_SECS_PER_TRACK) % ADF_HD_HEADS_PER_CYL;
    const size_t s = lba % ADF_HD_SECS_PER_TRACK;

    return FloppyDisk_ReadSector(self, h, c, s, pBuffer);
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t FloppyDisk_putBlock(FloppyDiskRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    // XXX hardcoded to HD for now
    const size_t c = lba / (ADF_HD_HEADS_PER_CYL * ADF_HD_SECS_PER_TRACK);
    const size_t h = (lba / ADF_HD_SECS_PER_TRACK) % ADF_HD_HEADS_PER_CYL;
    const size_t s = lba % ADF_HD_SECS_PER_TRACK;
    
    return FloppyDisk_WriteSector(self, h, c, s, pBuffer);
}



CLASS_METHODS(FloppyDisk, DiskDriver,
OVERRIDE_METHOD_IMPL(deinit, FloppyDisk, Object)
OVERRIDE_METHOD_IMPL(getBlockSize, FloppyDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlockCount, FloppyDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(isReadOnly, FloppyDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(getBlock, FloppyDisk, DiskDriver)
OVERRIDE_METHOD_IMPL(putBlock, FloppyDisk, DiskDriver)
);
