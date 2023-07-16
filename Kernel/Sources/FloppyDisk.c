//
//  FloppyDisk.c
//  Apollo
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDisk.h"
#include "Heap.h"
#include "Platform.h"
#include "SystemGlobals.h"
#include "VirtualProcessor.h"


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



extern UInt fdc_get_drive_status(FdcControlByte* _Nonnull fdc);
extern void fdc_set_drive_motor(FdcControlByte* _Nonnull fdc, Int onoff);
extern void fdc_step_head(FdcControlByte* _Nonnull fdc, Int inout);
extern void fdc_select_head(FdcControlByte* _Nonnull fdc, Int side);
extern void fdc_io_begin(FdcControlByte* _Nonnull fdc, UInt16* pData, Int nwords, Int readwrite);
extern UInt fdc_get_io_status(FdcControlByte* _Nonnull fdc);
extern void fdc_io_end(FdcControlByte*  _Nonnull fdc);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Floppy DMA
////////////////////////////////////////////////////////////////////////////////


// Returns the shared floppy DMA object
FloppyDMA* _Nonnull FloppyDMA_GetShared(void)
{
    return SystemGlobals_Get()->floppy_dma;
}

// Destroys the floppy DMA.
static void FloppyDMA_Destroy(FloppyDMA* _Nullable pDma)
{
    if (pDma) {
        if (pDma->irqHandler != 0) {
            InterruptController_RemoveInterruptHandler(InterruptController_GetShared(), pDma->irqHandler);
        }
        pDma->irqHandler = 0;
        
        Semaphore_Deinit(&pDma->inuse);
        Semaphore_Deinit(&pDma->done);
        
        kfree((Byte*)pDma);
    }
}

// Creates the floppy DMA singleton
FloppyDMA* _Nullable FloppyDMA_Create(void)
{
    FloppyDMA* pDma = (FloppyDMA*) kalloc_cleared(sizeof(FloppyDMA));
    FailNULL(pDma);

    Semaphore_Init(&pDma->inuse, 1);
    Semaphore_Init(&pDma->done, 0);
        
    pDma->irqHandler = InterruptController_AddSemaphoreInterruptHandler(InterruptController_GetShared(),
                                                                            INTERRUPT_ID_DISK_BLOCK,
                                                                            INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                                            &pDma->done);
    FailZero(pDma->irqHandler);
    InterruptController_SetInterruptHandlerEnabled(InterruptController_GetShared(), pDma->irqHandler, true);
    return pDma;

failed:
    FloppyDMA_Destroy(pDma);
    return NULL;
}

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
static ErrorCode FloppyDMA_DoIO(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, UInt16* _Nonnull pData, Int nwords, Int readwrite)
{
    ErrorCode err = EOK;
    
    Semaphore_Acquire(&pDma->inuse, kTimeInterval_Infinity);
    
    fdc_io_begin(pFdc, pData, nwords, 0);
    err = Semaphore_Acquire(&pDma->done, TimeInterval_MakeSeconds(10));
    if (err == EOK) {
        const UInt status = fdc_get_io_status(pFdc);
        
        if ((status & (1 << CIABPRA_BIT_DSKRDY)) != 0) {
            err = ENODRIVE;
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
    
    return (err == ETIMEOUT) ? ENODRIVE : err;
}

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
static ErrorCode FloppyDMA_Read(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, UInt16* _Nonnull pData, Int nwords)
{
    return FloppyDMA_DoIO(pDma, pFdc, pData, nwords, 0);
}

// Synchronously writes 'nwords' 16bit words from the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred to
// disk.
static ErrorCode FloppyDMA_Write(FloppyDMA* _Nonnull pDma, FdcControlByte* _Nonnull pFdc, const UInt16* _Nonnull pData, Int nwords)
{
    return FloppyDMA_DoIO(pDma, pFdc, (UInt16*)pData, nwords, 1);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: MFM decoding
////////////////////////////////////////////////////////////////////////////////


#define MFM_SYNC_WORD   0x4489
#define ADF_FORMAT_V1   0xff

typedef struct _ADF_SectorHeader
{
    UInt8   format;     // Amiga 1.0 format 0xff
    UInt8   track;
    UInt8   sector;
    UInt8   seceow;
    UInt32  zero;
    UInt32  header_crc;
    UInt32  data_crc;
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
static void mfm_decode_sector(const UInt32* input, UInt32* output, Int data_size)
{
#define MASK 0x55555555    /* 01010101 ... 01010101 */
    UInt32 odd_bits, even_bits;
    UInt32 chksum = 0;
    
    /* the decoding is made here long by long : with data_size/4 iterations */
    for (Int count = 0; count < data_size; count++) {
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
static void mfm_encode_sector(const UInt32* input, UInt32* output, Int data_size)
{
    UInt32* output_odd = output;
    UInt32* output_even = output + data_size;
    
    for (Int count = 0; count < data_size; count++) {
        const UInt32 data = *input;
        UInt32 odd_bits = 0;
        UInt32 even_bits = 0;
        UInt32 prev_odd_bit = 0;
        UInt32 prev_even_bit = 0;
        
        //    user's data bit      MFM coded bits
        //    ---------------      --------------
        //    1                    01
        //    0                    10 if following a 0 data bit
        //    0                    00 if following a 1 data bit
        for (Int8 i_even = 30; i_even >= 0; i_even -= 2) {
            const Int8 i_odd = i_even + 1;
            const UInt32 cur_odd_bit = data & (1 << i_odd);
            const UInt32 cur_even_bit = data & (1 << i_even);
            
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


// Invalidates the track cache.
static void FloppyDisk_InvalidateTrackBuffer(FloppyDisk* _Nonnull pDisk)
{
    if ((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0) {
        pDisk->flags &= ~FLOPPY_FLAG_TRACK_BUFFER_VALID;
        
        for (Int i = 0; i < FLOPPY_SECTORS_CAPACITY; i++) {
            pDisk->track_sectors[i] = 0;
        }
    }
}

// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
FloppyDisk* _Nullable FloppyDisk_Create(Int drive)
{
    FloppyDisk* pDisk = (FloppyDisk*)kalloc_cleared(sizeof(FloppyDisk));
    FailNULL(pDisk);
    
    pDisk->track_buffer = (UInt16*)kalloc_options(sizeof(UInt16) * FLOPPY_TRACK_BUFFER_CAPACITY, HEAP_ALLOC_OPTION_CHIPSET);
    FailNULL(pDisk->track_buffer);
    
    pDisk->track_size = FLOPPY_TRACK_BUFFER_CAPACITY;
    pDisk->head = -1;
    pDisk->cylinder = -1;
    pDisk->drive = drive;
    pDisk->ciabprb = 0xf9;     // motor off; all drives deselected; head 0; stepping off
    pDisk->ciabprb &= ~(1 << ((drive & 0x03) + 3));    // select this drive
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    return pDisk;
    
failed:
    FloppyDisk_Destroy(pDisk);
    return NULL;
}

void FloppyDisk_Destroy(FloppyDisk* _Nullable pDisk)
{
    if (pDisk) {
        kfree((Byte*)pDisk->track_buffer);
        pDisk->track_buffer = NULL;
        
        kfree((Byte*)pDisk);
    }
}

// Computes and returns the floppy status from the given fdc drive status.
static inline ErrorCode FloppyDisk_StatusFromDriveStatus(UInt drvstat)
{
    if ((drvstat & (1 << CIABPRA_BIT_DSKRDY)) != 0) {
        return ENODRIVE;
    }
    if ((drvstat & (1 << CIABPRA_BIT_DSKCHANGE)) == 0) {
        return EDISKCHANGE;
    }
    
    return EOK;
}

// Waits until the drive is ready (motor is spinning at full speed). This function
// waits for at most 500ms for the disk to become ready.
// Returns S_OK if the drive is ready; ETIMEOUT  if the drive failed to become
// ready in time.
static ErrorCode FloppyDisk_WaitDriveReady(FloppyDisk* _Nonnull pDisk)
{
    for (Int ntries = 0; ntries < 50; ntries++) {
        const UInt status = fdc_get_drive_status(&pDisk->ciabprb);
        
        if ((status & (1 << CIABPRA_BIT_DSKRDY)) == 0) {
            return EOK;
        }
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(10));
    }
    
    return ETIMEOUT;
}

// Seeks to track #0 and selects head #0. Returns true if the function seeked at
// least once.
// Note that this function is expected to implicitly acknowledge a disk change if
// it has actually seeked.
static Bool FloppyDisk_SeekToTrack_0(FloppyDisk* _Nonnull pDisk)
{
    Bool did_step_once = false;
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(18));
    
    for(;;) {
        const UInt status = fdc_get_drive_status(&pDisk->ciabprb);
        
        if ((status & (1 << CIABPRA_BIT_DSKTRACK0)) == 0) {
            break;
        }
        
        fdc_step_head(&pDisk->ciabprb, -1);
        did_step_once = true;
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3));
    }
    fdc_select_head(&pDisk->ciabprb, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(15));
    
    pDisk->head = 0;
    pDisk->cylinder = 0;
    pDisk->flags &= ~FLOPPY_FLAG_PREV_STEP_INWARD;
    
    return did_step_once;
}

// Seeks to the specified cylinder and selects the specified drive head.
// (0: outermost, 80: innermost, +: inward, -: outward).
// Returns EDISKCHANGE if the disk has changed.
// Note that we purposefully treat a disk change as an error. We don't want to
// implicitly and accidently acknowledge a disk change as a side effect of seeking.
// The user of the API needs to become aware of the disk change so that he can actually
// handle it in a sensible way.
static ErrorCode FloppyDisk_SeekTo(FloppyDisk* _Nonnull pDisk, Int cylinder, Int head)
{
    const Int diff = cylinder - pDisk->cylinder;
    const Int cur_dir = (diff >= 0) ? 1 : -1;
    const Int last_dir = (pDisk->flags & FLOPPY_FLAG_PREV_STEP_INWARD) ? 1 : -1;
    const Int nsteps = abs(diff);
    const Bool change_side = (pDisk->head != head);
    
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    const Int seek_pre_wait_ms = (nsteps > 0 && cur_dir != last_dir) ? 18 : 0;
    const Int side_pre_wait_ms = 2;
    const Int pre_wait_ms = max(seek_pre_wait_ms, side_pre_wait_ms);
    
    if (pre_wait_ms > 0) {
        VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(pre_wait_ms));
    }
    
    
    // Seek if necessary
    if (nsteps > 0) {
        for (Int i = nsteps; i > 0; i--) {
            const ErrorCode status = FloppyDisk_GetStatus(pDisk);
            if (status != EOK) {
                return status;
            }
            
            fdc_step_head(&pDisk->ciabprb, cur_dir);
            pDisk->cylinder += cur_dir;
            
            if (cur_dir >= 0) {
                pDisk->flags |= FLOPPY_FLAG_PREV_STEP_INWARD;
            } else {
                pDisk->flags &= ~FLOPPY_FLAG_PREV_STEP_INWARD;
            }
            
            VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3));
        }
    }
    
    // Switch heads if necessary
    if (change_side) {
        fdc_select_head(&pDisk->ciabprb, head);
        pDisk->head = head;
    }
    
    // Seek settle time: 15ms
    // Head select settle time: 100us
    const Int seek_settle_us = (nsteps > 0) ? 15*1000 : 0;
    const Int side_settle_us = (change_side) ? 100 : 0;
    const Int settle_us = max(seek_settle_us, side_settle_us);
    
    if (settle_us > 0) {
        VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(settle_us));
    }
    
    return EOK;
}

// Resets the floppy drive. This function figures out whether there is an actual
// physical floppy drive connected and whether it responds to commands and it then
// moves the disk head to track #0.
// Note that this function leaves the floppy motor turned on and that it implicitly
// acknowledges any pending disk change.
// Upper layer code should treat this function like a disk change.
void FloppyDisk_Reset(FloppyDisk* _Nonnull pDisk)
{
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    pDisk->head = -1;
    pDisk->cylinder = -1;
    
    
    // Turn the motor on to see whether there is an actual drive connected
    FloppyDisk_MotorOn(pDisk);
    const ErrorCode status = FloppyDisk_GetStatus(pDisk);
    if (status != EOK) {
        return;
    }
    
    
    // Move the head to track #0
    const Bool did_step_once = FloppyDisk_SeekToTrack_0(pDisk);
    
    
    // We didn't seek if we were already at track #0. So step to track #1 and
    // then back to #0 to acknowledge a disk change.
    if (!did_step_once) {
        fdc_step_head(&pDisk->ciabprb,  1);
        fdc_step_head(&pDisk->ciabprb, -1);
    }
}

// Returns the current floppy drive status.
ErrorCode FloppyDisk_GetStatus(FloppyDisk* _Nonnull pDisk)
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
void FloppyDisk_AcknowledgeDiskChange(FloppyDisk* _Nonnull pDisk)
{
    // Step by one track. This clears the disk change drive state if there is a
    // disk in the drive. If the disk change state doesn't change after the seek
    // then this means that there is truly no disk in the drive.
    // Also invalidate the cache 'cause it is certainly no longer valid.
    FloppyDisk_InvalidateTrackBuffer(pDisk);
    
    const Int dir = (pDisk->cylinder == 0) ? 1 : -1;
    fdc_step_head(&pDisk->ciabprb, dir);
}

// Turns the drive motor on and blocks the caller until the disk is ready.
void FloppyDisk_MotorOn(FloppyDisk* _Nonnull pDisk)
{
    fdc_set_drive_motor(&pDisk->ciabprb, 1);
    
    const ErrorCode err = FloppyDisk_WaitDriveReady(pDisk);
    if (err == ETIMEOUT) {
        fdc_set_drive_motor(&pDisk->ciabprb, 0);
        return;
    }
}

// Turns the drive motor off.
void FloppyDisk_MotorOff(FloppyDisk* _Nonnull pDisk)
{
    fdc_set_drive_motor(&pDisk->ciabprb, 0);
}

static ErrorCode FloppyDisk_ReadTrack(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder)
{
    ErrorCode status = EOK;
    
    // Seek to the required cylinder and select the required head
    if (pDisk->cylinder != cylinder || pDisk->head != head) {
        status = FloppyDisk_SeekTo(pDisk, cylinder, head);
        if (status != EOK) {
            return status;
        }
    }
    
    
    // Nothing to do if we already have this track cached in the track buffer
    if ((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0) {
        return EOK;
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    status = FloppyDisk_GetStatus(pDisk);
    if (status != EOK) {
        return status;
    }
    
    
    // Read the track
    status = FloppyDMA_Read(FloppyDMA_GetShared(), &pDisk->ciabprb, pDisk->track_buffer, pDisk->track_size);
    if (status != EOK) {
        return status;
    }
    
    
    // Clear out the sector table
    UInt16* track_buffer = pDisk->track_buffer;
    Int16* sector_table = pDisk->track_sectors;
    const Int16 track_buffer_size = pDisk->track_size;
    
    for (Int i = 0; i < FLOPPY_SECTORS_CAPACITY; i++) {
        sector_table[i] = 0;
    }
    
    
    // Build the sector table
    for (Int16 i = 0; i < track_buffer_size; i++) {
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
        mfm_decode_sector((const UInt32*)&track_buffer[i], (UInt32*)&header.format, 1);
        
        // Validate the sector header. We record valid sectors only.
        if (header.format != ADF_FORMAT_V1 || header.track != cylinder || header.sector >= ADF_DD_SECS_PER_CYL) {
            continue;
        }
        
        // Record the sector. Note that a sector may appear more than once because
        // we may have read more data from the disk than fits in a single track. We
        // keep the first occurence of a sector.
        if (sector_table[header.sector] == 0) {
            sector_table[header.sector] = i;
        }
    }
    
    pDisk->flags |= FLOPPY_FLAG_TRACK_BUFFER_VALID;
    
    return EOK;
}

ErrorCode FloppyDisk_ReadSector(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder, Int sector, Byte* _Nonnull pBuffer)
{
    ErrorCode err;
    
    // Read the track
    err = FloppyDisk_ReadTrack(pDisk, head, cylinder);
    if (err != EOK) {
        return err;
    }
    
    
    // Get the sector
    const UInt16* track_buffer = pDisk->track_buffer;
    const Int16* sector_table = pDisk->track_sectors;
    const Int16 idx = sector_table[sector];
    if (idx == 0) {
        return ENODATA;
    }
    
    
    // MFM decode the sector data
    mfm_decode_sector((const UInt32*)&track_buffer[idx + 28], (UInt32*)pBuffer, ADF_SECTOR_SIZE / sizeof(UInt32));
    
    /*
     for(Int i = 0; i < ADF_HD_SECS_PER_CYL; i++) {
     Int offset = sector_table[i];
     
     if (offset != 0) {
     print("H: %d, C: %d, S: %d -> %d  (0x%x)\n", head, cylinder, i, offset, &track_buffer[offset]);
     }
     }
     */
    return EOK;
}

static ErrorCode FloppyDisk_WriteTrack(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder)
{
    ErrorCode status = EOK;
    
    // There must be a valid track cache
    assert((pDisk->flags & FLOPPY_FLAG_TRACK_BUFFER_VALID) != 0);
    
    
    // Seek to the required cylinder and select the required head
    if (pDisk->cylinder != cylinder || pDisk->head != head) {
        status = FloppyDisk_SeekTo(pDisk, cylinder, head);
        if (status != EOK) {
            return status;
        }
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    status = FloppyDisk_GetStatus(pDisk);
    if (status != EOK) {
        return status;
    }
    
    
    // write the track
    status = FloppyDMA_Write(FloppyDMA_GetShared(), &pDisk->ciabprb, pDisk->track_buffer, pDisk->track_size);
    if (status != EOK) {
        return status;
    }
    
    return EOK;
}

ErrorCode FloppyDisk_WriteSector(FloppyDisk* _Nonnull pDisk, Int head, Int cylinder, Int sector, const Byte* pBuffer)
{
    ErrorCode err;
    
    // Make sure that we have the track in memory
    err = FloppyDisk_ReadTrack(pDisk, head, cylinder);
    if (err != EOK) {
        return err;
    }
    
    
    // Override the sector with the new data
    UInt16* track_buffer = pDisk->track_buffer;
    const Int16* sector_table = pDisk->track_sectors;
    const Int16 idx = sector_table[sector];
    if (idx == 0) {
        return ENODATA;
    }
    
    
    // MFM encode the sector data
    mfm_encode_sector((const UInt32*)pBuffer, (UInt32*)&track_buffer[idx + 28], ADF_SECTOR_SIZE / sizeof(UInt32));
    
    
    // Write the track back out
    // XXX should just mark the track buffer as dirty
    // XXX the cache_invalidate() function should then write the cache back to disk before we seek / switch heads
    // XXX there should be a floppy_is_cache_dirty() and floppy_flush_cache() which writes the cache to disk
    FloppyDisk_WriteTrack(pDisk, head, cylinder);
    
    return EOK;
}
