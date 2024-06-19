//
//  FloppyDisk.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDiskPriv.h"
#include <dispatcher/VirtualProcessor.h>
#include <hal/Platform.h>
#include "mfm.h"

static errno_t FloppyDisk_Create(int drive, FloppyController* _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk);
static errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull self);
static void FloppyDisk_InvalidateReadTrack(FloppyDiskRef _Nonnull pDisk);
static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self);


errno_t FloppyDisk_DiscoverDrives(FloppyDiskRef _Nullable pOutDrives[MAX_FLOPPY_DISK_DRIVES])
{
    decl_try_err();
    FloppyController* fdc = NULL;
    int nDrivesOkay = 0;

    for (int i = 0; i < MAX_FLOPPY_DISK_DRIVES; i++) {
        pOutDrives[i] = NULL;
    }


    err = FloppyController_Create(&fdc);
    if (err != EOK) {
        return err;
    }

    for (int i = 0; i < 1 /*MAX_FLOPPY_DISK_DRIVES*/; i++) {
        errno_t err0 = FloppyDisk_Create(i, fdc, &pOutDrives[i]);
        if (err0 == EOK) {
            err0 = FloppyDisk_Reset(pOutDrives[i]);
        }

        if (err0 != EOK && nDrivesOkay == 0) {
            err = err0;
        }
    }

    return (nDrivesOkay > 0) ? EOK : err;
}


////////////////////////////////////////////////////////////////////////////////



// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
static errno_t FloppyDisk_Create(int drive, FloppyController* _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk)
{
    decl_try_err();
    FloppyDisk* self;
    
    try(Object_Create(FloppyDisk, &self));
    
    self->fdc = pFdc;

    // XXX hardcoded to DD for now
    self->logicalBlockCapacity = ADF_DD_HEADS_PER_CYL*ADF_DD_CYLS_PER_DISK*ADF_DD_SECS_PER_TRACK;
    self->sectorsPerCylinder = ADF_DD_HEADS_PER_CYL * ADF_DD_SECS_PER_TRACK;
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;
    self->headsPerCylinder = ADF_DD_HEADS_PER_CYL;
    self->trackWordCountToRead = ADF_DD_TRACK_WORD_COUNT_TO_READ;
    self->trackWordCountToWrite = ADF_DD_TRACK_WORD_COUNT_TO_WRITE;

    try(kalloc_options(sizeof(uint16_t) * self->trackWordCountToRead, KALLOC_OPTION_UNIFIED, (void**) &self->trackBuffer));

    self->head = -1;
    self->cylinder = -1;
    self->drive = drive;
    self->ciabprb = 0xf9;     // motor off; all drives deselected; head 0; stepping off
    self->ciabprb &= ~(1 << ((drive & 0x03) + 3));    // select this drive
    
    FloppyDisk_InvalidateReadTrack(self);
    
    *pOutDisk = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutDisk = NULL;
    return err;
}

static void FloppyDisk_deinit(FloppyDiskRef _Nonnull self)
{
    kfree(self->trackBuffer);
    self->trackBuffer = NULL;

    self->fdc = NULL;
}

// Invalidates the read track cache.
static void FloppyDisk_InvalidateReadTrack(FloppyDiskRef _Nonnull self)
{
    if (self->flags.isReadTrackValid) {
        self->flags.isReadTrackValid = 0;
        memset(self->sectors, 0, sizeof(ADF_MFMSector*) * ADF_HD_SECS_PER_TRACK);
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
static errno_t FloppyDisk_WaitDriveReady(FloppyDiskRef _Nonnull self)
{
    decl_try_err();

    for (int ntries = 0; ntries < 50; ntries++) {
        const unsigned int status = fdc_get_drive_status(&self->ciabprb);
        
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
static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull self)
{
    decl_try_err();
    bool did_step_once = false;
    
    FloppyDisk_InvalidateReadTrack(self);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(18)));
    
    for(;;) {
        const unsigned int status = fdc_get_drive_status(&self->ciabprb);
        
        if ((status & (1 << CIABPRA_BIT_DSKTRACK0)) == 0) {
            break;
        }
        
        fdc_step_head(&self->ciabprb, -1);
        did_step_once = true;
        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3)));
    }
    fdc_select_head(&self->ciabprb, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(15)));
    
    self->head = 0;
    self->cylinder = 0;
    self->flags.wasMostRecentSeekInward = 0;
    
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
static errno_t FloppyDisk_SeekTo(FloppyDiskRef _Nonnull self, int cylinder, int head)
{
    decl_try_err();
    const int diff = cylinder - self->cylinder;
    const int cur_dir = (diff >= 0) ? 1 : -1;
    const int last_dir = (self->flags.wasMostRecentSeekInward) ? 1 : -1;
    const int nsteps = __abs(diff);
    const bool change_side = (self->head != head);

//    print("*** SeekTo(c: %d, h: %d)\n", cylinder, head);
    FloppyDisk_InvalidateReadTrack(self);
    
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
            try(FloppyDisk_GetStatus(self));
            
            fdc_step_head(&self->ciabprb, cur_dir);
            self->cylinder += cur_dir;
            
            if (cur_dir >= 0) {
                self->flags.wasMostRecentSeekInward = 1;
            } else {
                self->flags.wasMostRecentSeekInward = 0;
            }
            
            try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3)));
        }
    }
    
    // Switch heads if necessary
    if (change_side) {
        fdc_select_head(&self->ciabprb, head);
        self->head = head;
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
static errno_t FloppyDisk_Reset(FloppyDiskRef _Nonnull self)
{
    decl_try_err();

    FloppyDisk_InvalidateReadTrack(self);
    self->head = -1;
    self->cylinder = -1;
    self->readErrorCount = 0;
    
    
    // Turn the motor on to see whether there is an actual drive connected
    FloppyDisk_MotorOn(self);
    try(FloppyDisk_GetStatus(self));
    
    
    // Move the head to track #0
    err = FloppyDisk_SeekToTrack_0(self);
    
    
    // We didn't seek if we were already at track #0. So step to track #1 and
    // then back to #0 to acknowledge a disk change.
    if (err != EOK) {
        fdc_step_head(&self->ciabprb,  1);
        fdc_step_head(&self->ciabprb, -1);
    }

    return EOK;

catch:
    return err;
}

// Returns the current floppy drive status.
errno_t FloppyDisk_GetStatus(FloppyDiskRef _Nonnull self)
{
    return FloppyDisk_StatusFromDriveStatus(fdc_get_drive_status(&self->ciabprb));
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
void FloppyDisk_AcknowledgeDiskChange(FloppyDiskRef _Nonnull self)
{
    // Step by one track. This clears the disk change drive state if there is a
    // disk in the drive. If the disk change state doesn't change after the seek
    // then this means that there is truly no disk in the drive.
    // Also invalidate the cache 'cause it is certainly no longer valid.
    FloppyDisk_InvalidateReadTrack(self);
    self->readErrorCount = 0;

    const int dir = (self->cylinder == 0) ? 1 : -1;
    fdc_step_head(&self->ciabprb, dir);
}

// Turns the drive motor on and blocks the caller until the disk is ready.
static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self)
{
    fdc_set_drive_motor(&self->ciabprb, 1);
    
    const errno_t err = FloppyDisk_WaitDriveReady(self);
    if (err == ETIMEDOUT) {
        fdc_set_drive_motor(&self->ciabprb, 0);
        return;
    }
}

// Turns the drive motor off.
static void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull self)
{
    fdc_set_drive_motor(&self->ciabprb, 0);
}

enum {
    kScanMode_Scanning,     // Scan until we find a ADF_MFM_SYNC word (inside the gap)
    kScanMode_Sectoring,    // Picking up expected sectors (outside the gap)
};

static errno_t FloppyDisk_ReadTrack(FloppyDiskRef _Nonnull self, int head, int cylinder)
{
    decl_try_err();
    
    // Seek to the required cylinder and select the required head
    if (self->cylinder != cylinder || self->head != head) {
        try(FloppyDisk_SeekTo(self, cylinder, head));
    }
    
    
    // Nothing to do if we already have this track cached in the track buffer
    if (self->flags.isReadTrackValid) {
        return EOK;
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    try(FloppyDisk_GetStatus(self));
    
    
    // Read the track
    try(FloppyController_Read(self->fdc, &self->ciabprb, self->trackBuffer, self->trackWordCountToRead));
    
    
    
    // Build the sector table
    const uint16_t* pt = self->trackBuffer;
    const uint16_t* pt_limit = &self->trackBuffer[self->trackWordCountToRead];
    const uint16_t* gap_start = NULL;
    int mode = kScanMode_Sectoring, valid_sector_count = 0;
    ADF_SectorInfo info;

   // print("start: %p, limit: %p\n", pt, pt_limit);
    while (pt < pt_limit && valid_sector_count < self->sectorsPerTrack) {
        if (mode == kScanMode_Sectoring) {
            // Skip 1 or 2 sync words. 1 word appears in front of the first sector
            // that Paula returns and in front of the first sector following the
            // gap. 2 sync words appear in front of all other sectors.
            pt++;
            if (pt < pt_limit && *pt == ADF_MFM_SYNC) {
                pt++;
            }

            if ((pt + ADF_MFM_SECTOR_SIZE / 2) > pt_limit) {
                break;
            }

            // MFM decode the sector header
            mfm_decode_sector((const uint32_t*)pt, (uint32_t*)&info, 1);

            // Validate the sector header. We record valid sectors only.
            if (info.format != ADF_FORMAT_V1 || info.track != 2*cylinder + head || info.sector >= self->sectorsPerTrack) {
                // Bad sector
                pt += ADF_MFM_SECTOR_SIZE / 2;
                continue;
            }

            // Record the sector. Note that a sector may appear more than once because
            // we may have read more data from the disk than fits in a single track. We
            // keep the first occurrence of a sector.
            if (self->sectors[info.sector] == NULL) {
                #if 0
                if (info.sectors_until_gap == 1) {
                    print("sector: %d, at: %p -- gap starts here\n", info.sector, pt);
                } else {
                    print("sector: %d, at: %p\n", info.sector, pt);
                }
                #endif
                self->sectors[info.sector] = (ADF_MFMSector*)pt;
            }

            pt += ADF_MFM_SECTOR_SIZE / 2;
            pt += 4 / 2;

            if (info.sectors_until_gap == 1) {
                // We've reached the start pf the gap. Switch to scanning until
                // we find a sync word
                mode = kScanMode_Scanning;
                gap_start = pt;
            }
        }
        else {
            // we scan until we find 1 sync word
            if (*pt == ADF_MFM_SYNC) {
                mode = kScanMode_Sectoring;
                self->gapSize = (pt - gap_start) * sizeof(uint16_t);
            }
            else {
                pt++;
            }
        }
    }
    
    self->flags.isReadTrackValid = 1;
    
    return EOK;

catch:
    return err;
}

static errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, void* _Nonnull pBuffer)
{
    decl_try_err();
    
    // Read the track
    try(FloppyDisk_ReadTrack(self, head, cylinder));
    
    
    // Get the sector
    ADF_MFMSector* psec = self->sectors[sector];
    if (psec == NULL) {
        self->readErrorCount++;
        return EIO;
    }
    
    
    // MFM decode the sector data
    mfm_decode_sector((const uint32_t*)psec->data.odd_bits, (uint32_t*)pBuffer, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
    return EOK;

catch:
    return err;
}

static errno_t FloppyDisk_WriteTrack(FloppyDiskRef _Nonnull self, int head, int cylinder)
{
    decl_try_err();
    
    // There must be a valid track cache
    assert(self->flags.isReadTrackValid);
    
    
    // Seek to the required cylinder and select the required head
    if (self->cylinder != cylinder || self->head != head) {
        try(FloppyDisk_SeekTo(self, cylinder, head));
    }
    
    
    // Validate that the drive is still there, motor turned on and that there was
    // no disk change
    try(FloppyDisk_GetStatus(self));
    
    
    // write the track
    try(FloppyController_Write(self->fdc, &self->ciabprb, self->trackBuffer, self->trackWordCountToWrite));
    
    return EOK;

catch:
    return err;
}

static errno_t FloppyDisk_WriteSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, const void* pBuffer)
{
#if 0
    decl_try_err();
    
    // Make sure that we have the track in memory
    try(FloppyDisk_ReadTrack(self, head, cylinder));
    
    
    // Override the sector with the new data
    const ADF_MFMSector* psec = self->sectors[sector];
    if (psec == NULL) {
        return EIO;
    }
    
    
    // MFM encode the sector data
    mfm_encode_sector((const uint32_t*)pBuffer, (uint32_t*)psec->data.odd_bits, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
    
    
    // Write the track back out
    // XXX should just mark the track buffer as dirty
    // XXX the cache_invalidate() function should then write the cache back to disk before we seek / switch heads
    // XXX there should be a floppy_is_cache_dirty() and floppy_flush_cache() which writes the cache to disk
    try(FloppyDisk_WriteTrack(self, head, cylinder));
    
    return EOK;

catch:
    return err;
#else
    return EROFS;
#endif
}

// Returns the size of a block.
size_t FloppyDisk_getBlockSize(FloppyDiskRef _Nonnull self)
{
    return ADF_SECTOR_DATA_SIZE;
}

// Returns the number of blocks that the disk is able to store.
LogicalBlockCount FloppyDisk_getBlockCount(FloppyDiskRef _Nonnull self)
{
    return self->logicalBlockCapacity;
}

// Returns true if the disk if read-only.
bool FloppyDisk_isReadOnly(FloppyDiskRef _Nonnull self)
{
    return FloppyController_IsReadOnly(self->fdc, self->drive);
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
errno_t FloppyDisk_getBlock(FloppyDiskRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->logicalBlockCapacity) {
        return EIO;
    }

    const int c = lba / self->sectorsPerCylinder;
    const int h = (lba / self->sectorsPerTrack) % self->headsPerCylinder;
    const int s = lba % self->sectorsPerTrack;

//    print("lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);
    return FloppyDisk_ReadSector(self, h, c, s, pBuffer);
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
errno_t FloppyDisk_putBlock(FloppyDiskRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    if (lba >= self->logicalBlockCapacity) {
        return EIO;
    }

    const int c = lba / self->sectorsPerCylinder;
    const int h = (lba / self->sectorsPerTrack) % self->headsPerCylinder;
    const int s = lba % self->sectorsPerTrack;

//    print("WRITE: lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);

    return FloppyDisk_WriteSector(self, h, c, s, pBuffer);
}



class_func_defs(FloppyDisk, DiskDriver,
override_func_def(deinit, FloppyDisk, Object)
override_func_def(getBlockSize, FloppyDisk, DiskDriver)
override_func_def(getBlockCount, FloppyDisk, DiskDriver)
override_func_def(isReadOnly, FloppyDisk, DiskDriver)
override_func_def(getBlock, FloppyDisk, DiskDriver)
override_func_def(putBlock, FloppyDisk, DiskDriver)
);
