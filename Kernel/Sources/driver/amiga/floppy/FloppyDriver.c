//
//  FloppyDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDriverPriv.h"
#include <dispatcher/VirtualProcessor.h>
#include <dispatchqueue/DispatchQueue.h>
#include <hal/MonotonicClock.h>
#include <hal/Platform.h>
#include <log/Log.h>
#include <kern/kalloc.h>
#include <kern/string.h>
#include <kern/timespec.h>
#include "mfm.h"


// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
errno_t FloppyDriver_Create(DriverRef _Nullable parent, int drive, DriveState ds, const DriveParams* _Nonnull params, FloppyDriverRef _Nullable * _Nonnull pOutDisk)
{
    decl_try_err();
    FloppyDriverRef self;

    try(DiskDriver_Create(class(FloppyDriver), 0, parent, (DriverRef*)&self));

    self->drive = drive;
    self->driveState = ds;
    self->params = params;

    self->head = -1;
    self->cylinder = -1;
    self->readErrorCount = 0;

    self->flags.motorState = kMotor_Off;
    self->flags.wasMostRecentSeekInward = 0;
    self->flags.shouldResetDiskChangeStepInward = 0;
    self->flags.isOnline = 0;

    *pOutDisk = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutDisk = NULL;
    return err;
}

static void FloppyDriver_deinit(FloppyDriverRef _Nonnull self)
{
    FloppyDriver_MotorOff(self);

    kfree(self->dmaBuffer);
    self->dmaBuffer = NULL;

    kfree(self->sectorCache);
    self->sectorCache = NULL;
}

static void _FloppyDriver_doSenseDisk(FloppyDriverRef _Nonnull self)
{
    decl_try_err();
    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    bool didStep;
    bool hasDiskChangeDetected = false;

    if (!self->flags.didResetDrive) {
        self->flags.didResetDrive = 1;
        self->scTrackNo = -1;

        err = FloppyDriver_SeekToTrack_0(self, &didStep);
        if (err != EOK) {
            FloppyDriver_OnHardwareLost(self);
            return;
        }

        self->flags.isOnline = 1;
        hasDiskChangeDetected = true;
    }

    if ((FloppyController_GetStatus(fdc, self->driveState) & kDriveStatus_DiskChanged) != 0) {
        FloppyDriver_ResetDriveDiskChange(self);
        hasDiskChangeDetected = true;
    }


    if (hasDiskChangeDetected || DiskDriver_IsDiskChangePending(self)) {
        const uint8_t status = FloppyController_GetStatus(fdc, self->driveState);
        const unsigned int hasDisk = ((status & kDriveStatus_DiskChanged) == 0) ? 1 : 0;

        if (hasDisk) {
            SensedDisk info;
    
            info.properties = kMediaProperty_IsRemovable;
            if ((status & kDriveStatus_IsReadOnly) == kDriveStatus_IsReadOnly) {
                info.properties |= kMediaProperty_IsReadOnly;
            }
            info.sectorSize = ADF_SECTOR_DATA_SIZE;
            info.heads = ADF_HEADS_PER_CYL;
            info.cylinders = self->params->cylinders;
            info.sectorsPerTrack = self->sectorsPerTrack;
            info.rwClusterSize = self->sectorsPerTrack;
            DiskDriver_NoteSensedDisk((DiskDriverRef)self, &info);
        }
        else {
            self->scTrackNo = -1;
            DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
        }
    }
}

void FloppyDriver_doSenseDisk(FloppyDriverRef _Nonnull self, SenseDiskRequest* _Nonnull req)
{
    _FloppyDriver_doSenseDisk(self);
}

static void FloppyDriver_Reset(FloppyDriverRef _Nonnull self)
{
    // XXX hardcoded to DD for now
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;

    self->dmaReadWordCount = DMA_BYTE_SIZE(self->sectorsPerTrack) / 2;
    self->dmaWriteWordCount = self->dmaReadWordCount + ADF_MFM_SYNC_SIZE/2; /* +2 for the 3bit loss on write DMA bug*/
    assert(kalloc_options(sizeof(uint16_t) * self->dmaWriteWordCount, KALLOC_OPTION_UNIFIED, (void**) &self->dmaBuffer) == EOK);

    self->scTrackNo = -1;
    assert(kalloc_options(SECTOR_CACHE_BYTE_SIZE(self->sectorsPerTrack), 0, (void**) &self->sectorCache) == EOK);

    self->head = -1;
    self->cylinder = -1;


    _FloppyDriver_doSenseDisk(self);
}

errno_t FloppyDriver_onStart(FloppyDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    char name[4];

    name[0] = 'f';
    name[1] = 'd';
    name[2] = '0' + self->drive;
    name[3] = '\0';

    DriverEntry de;
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    if ((err = Driver_Publish((DriverRef)self, &de)) == EOK) {
        if ((err = DispatchQueue_DispatchAsync(DiskDriver_GetDispatchQueue(self), (VoidFunc_1)FloppyDriver_Reset, self)) == EOK) {
            return EOK;
        }

        Driver_Unpublish((DriverRef)self);
    }

    return err;
}

// Called when we've detected a loss of the drive hardware
static void FloppyDriver_OnHardwareLost(FloppyDriverRef _Nonnull self)
{
    FloppyDriver_MotorOff(self);
    self->scTrackNo = -1;
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
    self->flags.isOnline = 0;
}


////////////////////////////////////////////////////////////////////////////////
// Motor Control
////////////////////////////////////////////////////////////////////////////////

static void FloppyDriver_CancelDelayedMotorOff(FloppyDriverRef _Nonnull self)
{
    DispatchQueue_RemoveByTag(DiskDriver_GetDispatchQueue(self), kDelayedMotorOffTag);
}

// Turns the drive motor off.
static void FloppyDriver_MotorOff(FloppyDriverRef _Nonnull self)
{
    // Note: may be called if the motor went off on us without our doing. We call
    // this function in this case to resync out software state with the hardware
    // state.
    if (self->flags.isOnline) {
        FloppyController_SetMotor(FloppyDriver_GetController(self), &self->driveState, false);
    }
    self->flags.motorState = kMotor_Off;

    FloppyDriver_CancelDelayedMotorOff(self);
}

// Turns the drive motor on and schedules an auto-motor-off in 4 seconds.
static void FloppyDriver_MotorOn(FloppyDriverRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_Off) {
        FloppyController_SetMotor(FloppyDriver_GetController(self), &self->driveState, true);
        self->flags.motorState = kMotor_SpinningUp;
    }

    FloppyDriver_CancelDelayedMotorOff(self);
    
    const struct timespec curTime = MonotonicClock_GetCurrentTime();
    const struct timespec deadline = timespec_add(curTime, timespec_from_sec(4));
    DispatchQueue_DispatchAsyncAfter(DiskDriver_GetDispatchQueue(self),
            deadline,
            (VoidFunc_1)FloppyDriver_MotorOff,
            self,
            kDelayedMotorOffTag);
}

// Waits until the drive is ready (motor is spinning at full speed). This function
// waits for at most 500ms for the disk to become ready.
// Returns EOK if the drive is ready; ETIMEDOUT if the drive failed to become
// ready in time.
static errno_t FloppyDriver_WaitForDiskReady(FloppyDriverRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_AtTargetSpeed) {
        return EOK;
    }
    else if (self->flags.motorState == kMotor_SpinningUp) {
        // Waits for at most 500ms for the motor to reach its target speed
        const struct timespec delay = timespec_from_ms(10);
        FloppyControllerRef fdc = FloppyDriver_GetController(self);

        for (int i = 0; i < 50; i++) {
            const uint8_t status = FloppyController_GetStatus(fdc, self->driveState);

            if ((status & kDriveStatus_DiskReady) != 0) {
                self->flags.motorState = kMotor_AtTargetSpeed;
                return EOK;
            }
            const errno_t err = VirtualProcessor_Sleep(delay);
            if (err != EOK) {
                return EIO;
            }
        }

        // Timed out. Turn the motor off for now so that another I/O request can
        // try spinning the motor up to its target speed again.
        FloppyDriver_MotorOff(self);
        return ETIMEDOUT;
    }
    else if (self->flags.motorState == kMotor_Off) {
        return EIO;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Seeking & Head Selection
////////////////////////////////////////////////////////////////////////////////

// Seeks to track #0 and selects head #0. Returns ETIMEDOUT if the seek failed
// because there's probably no drive connected.
static errno_t FloppyDriver_SeekToTrack_0(FloppyDriverRef _Nonnull self, bool* _Nonnull pOutDidStep)
{
    decl_try_err();
    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    int steps = 0;

    *pOutDidStep = false;

    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    try(VirtualProcessor_Sleep(timespec_from_ms(18)));
    
    while ((FloppyController_GetStatus(fdc, self->driveState) & kDriveStatus_AtTrack0) == 0) {
        FloppyController_StepHead(fdc, self->driveState, -1);
        
        steps++;
        if (steps > 80) {
            throw(ETIMEDOUT);
        }

        try(VirtualProcessor_Sleep(timespec_from_ms(3)));
    }
    FloppyController_SelectHead(fdc, &self->driveState, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    try(VirtualProcessor_Sleep(timespec_from_ms(15)));
    
    self->head = 0;
    self->cylinder = 0;
    self->flags.wasMostRecentSeekInward = 0;
    *pOutDidStep = (steps > 0) ? true : false;

catch:
    return err;
}

// Seeks to the specified cylinder and selects the specified drive head.
// (0: outermost, 79: innermost, +: inward, -: outward).
static errno_t FloppyDriver_SeekTo(FloppyDriverRef _Nonnull self, int cylinder, int head)
{
    decl_try_err();
    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    const int diff = cylinder - self->cylinder;
    const int cur_dir = (diff >= 0) ? 1 : -1;
    const int last_dir = (self->flags.wasMostRecentSeekInward) ? 1 : -1;
    const int nSteps = __abs(diff);
    const bool change_side = (self->head != head);

//    printf("*** SeekTo(c: %d, h: %d)\n", cylinder, head);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    const int seek_pre_wait_ms = (nSteps > 0 && cur_dir != last_dir) ? 18 : 0;
    const int side_pre_wait_ms = 2;
    const int pre_wait_ms = __max(seek_pre_wait_ms, side_pre_wait_ms);
    
    if (pre_wait_ms > 0) {
        try(VirtualProcessor_Sleep(timespec_from_ms(pre_wait_ms)));
    }
    
    
    // Seek if necessary
    if (nSteps > 0) {
        for (int i = nSteps; i > 0; i--) {
            FloppyController_StepHead(fdc, self->driveState, cur_dir);     
            
            self->cylinder += cur_dir;
            
            if (cur_dir >= 0) {
                self->flags.wasMostRecentSeekInward = 1;
            } else {
                self->flags.wasMostRecentSeekInward = 0;
            }
            
            try(VirtualProcessor_Sleep(timespec_from_ms(3)));
        }
    }
    
    // Switch heads if necessary
    if (change_side) {
        FloppyController_SelectHead(fdc, &self->driveState, head);
        self->head = head;
    }
    
    // Seek settle time: 15ms
    // Head select settle time: 100us
    const int seek_settle_us = (nSteps > 0) ? 15*1000 : 0;
    const int side_settle_us = (change_side) ? 100 : 0;
    const int settle_us = __max(seek_settle_us, side_settle_us);
    
    if (settle_us > 0) {
        try(VirtualProcessor_Sleep(timespec_from_us(settle_us)));
    }
    
catch:
    return err;
}

static void FloppyDriver_ResetDriveDiskChange(FloppyDriverRef _Nonnull self)
{
    // We have to step the disk head to trigger a reset of the disk change bit.
    // We do this in a smart way in the sense that we step back and forth while
    // maintaining the general location of the disk head. Ie disk head is at
    // cylinder 3 and there's no disk in the drive. We step 4, 3, 4, 3... until
    // a disk is inserted.
    int c = (self->flags.shouldResetDiskChangeStepInward) ? self->cylinder + 1 : self->cylinder - 1;

    if (c > self->params->cylinders - 1) {
        c = self->cylinder - 1;
        self->flags.shouldResetDiskChangeStepInward = 0;
    }
    else if (c < 0) {
        c = 1;
        self->flags.shouldResetDiskChangeStepInward = 1;
    }

    FloppyDriver_SeekTo(self, c, self->head);
    self->flags.shouldResetDiskChangeStepInward = !self->flags.shouldResetDiskChangeStepInward;
}


////////////////////////////////////////////////////////////////////////////////
// Disk I/O
////////////////////////////////////////////////////////////////////////////////

// Invoked at the beginning of a disk read/write operation to prepare the drive
// state. Ie turn motor on, seek, switch disk head, detect drive status, etc. 
static errno_t FloppyDriver_PrepareIO(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs)
{
    decl_try_err();
    FloppyControllerRef fdc = FloppyDriver_GetController(self);

    // Make sure we still got the drive hardware and that the disk hasn't changed
    // on us
    if (!self->flags.isOnline) {
        return ENODEV;
    }


    // Make sure that the motor is turned on
    FloppyDriver_MotorOn(self);


    // Seek to the required cylinder and select the required head
    if (self->cylinder != chs->c || self->head != chs->h) {
        err = FloppyDriver_SeekTo(self, chs->c, chs->h);
    }


    // Wait until the motor has reached its target speed
    if (err == EOK) {
        err = FloppyDriver_WaitForDiskReady(self);
    }

    return err;
}

// Invoked to do the actual read/write operation. Also validates that the disk
// hasn't been yanked out of the drive or changed on us while doing the I/O.
// Expects that the track buffer is properly prepared for the I/O.
static errno_t FloppyDriver_DoSyncIO(FloppyDriverRef _Nonnull self, bool bWrite)
{
    uint16_t precomp;
    int16_t nWords;

    if (bWrite) {
        if (self->cylinder <= self->params->precomp_00) {
            precomp = kPrecompensation_0ns;
        }
        else if (self->cylinder <= self->params->precomp_01) {
            precomp = kPrecompensation_140ns;
        }
        else if (self->cylinder <= self->params->precomp_10) {
            precomp = kPrecompensation_280ns;
        }
        else {
            precomp = kPrecompensation_560ns;
        }
        nWords = self->dmaWriteWordCount;
    }
    else {
        precomp = kPrecompensation_0ns;
        nWords = self->dmaReadWordCount;
    }

    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    return FloppyController_Dma(fdc, self->driveState, precomp, self->dmaBuffer, nWords, bWrite);
}

// Invoked at the end of a disk I/O operation. Potentially translates the provided
// internal error code to an external one and kicks of disk-change related flow
// control and initiates a delayed motor-off operation.
static errno_t FloppyDriver_FinalizeIO(FloppyDriverRef _Nonnull self, errno_t err)
{
    switch (err) {
        case EOK:
            break;

        case ETIMEDOUT:
            // A timeout may be caused by:
            // - no drive connected
            // - no disk in drive
            // - electro-mechanical problem
            FloppyDriver_OnHardwareLost(self);
            err = ENODEV;
            break;

        case EDISKCHANGE:
            FloppyDriver_MotorOff(self);
            DiskDriver_NoteDiskChanged((DiskDriverRef)self);
            break;

        default:
            err = EIO;
            break;
    }

    return err;
}


static void FloppyDriver_DecodeSector(FloppyDriverRef _Nonnull self, int16_t offset, uint8_t targetTrack)
{
    const ADF_MFMSector* mfmSector = (const ADF_MFMSector*)&self->dmaBuffer[offset];
    ADF_SectorInfo info;
    ADF_Checksum diskChecksum, myChecksum;

    // Decode the stored sector header checksum, calculate our checksum and make
    // sure that they match. This is not a valid sector if they don't match.
    // The header checksum is calculated based on:
    // - 2 MFM info longwords
    // - 8 MFM sector label longwords
    mfm_decode_bits(&mfmSector->header_checksum.odd_bits, &diskChecksum, 1);
    myChecksum = mfm_checksum(&mfmSector->info.odd_bits, 2 + 8);

    if (diskChecksum != myChecksum) {
        return;
    }


    // MFM decode the sector info long word
    mfm_decode_bits(&mfmSector->info.odd_bits, (uint32_t*)&info, 1);


    // Validate the sector info
    if (info.format != ADF_FORMAT_V1
        || info.track != targetTrack
        || info.sector >= self->sectorsPerTrack
        || info.sectors_until_gap > self->sectorsPerTrack) {
        return;
    }


    // Save the decoded header
    CachedSector* pcs = &self->sectorCache[info.sector];
    pcs->info.format = info.format;
    pcs->info.track = info.track;
    pcs->info.sector = info.sector;
    pcs->info.sectors_until_gap = info.sectors_until_gap;


    // Save the decoded label
    mfm_decode_bits(mfmSector->label.odd_bits, (uint32_t*)&pcs->label[0], 4);


    // Save the decoded sector data
    mfm_decode_bits(mfmSector->data.odd_bits, (uint32_t*)&pcs->data[0], ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));


    // Validate the sector data
    mfm_decode_bits(&mfmSector->data_checksum.odd_bits, &diskChecksum, 1);
    myChecksum = mfm_checksum(mfmSector->data.odd_bits, 256);

    pcs->state = (diskChecksum == myChecksum) ? kSectorState_Ok : kSectorState_BadDataChecksum;
}

static errno_t FloppyDriver_DecodeTrack(FloppyDriverRef _Nonnull self, uint8_t targetTrack)
{
    CachedSector* psc = self->sectorCache;
    const uint16_t* pt = self->dmaBuffer;
    const uint16_t* pt_start = pt;
    const uint16_t* pt_limit = &self->dmaBuffer[self->dmaReadWordCount];

    // Invalidate the sector cache
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        psc[i].state = kSectorState_Missing;
    }
    self->scTrackNo = -1;


    // Decode the sectors in the track and store them in the sector cache
    while (pt < pt_limit) {
        // Find the next MFM sync mark
        // We don't verify the pre-sync words. They may be 0x2AAA or 0xAAAA. Or
        // they are missing altogether because this is the first sector in the
        // track (also saw missing pre-sync words for first sector after the
        // track gap in WinUAE).
        // We don't mandate 2 0x4489 in a row because we sometimes get just one
        // 0x4489. I.e. the first sector read in and the first sector following
        // the track gap. However, with the track gap you sometimes get 2 0x4489
        // and sometimes just one 0x4489... (this may be WinUAE specific too)
        while (pt < pt_limit) {
            if (*pt++ == ADF_MFM_SYNC) {
                if (*pt == ADF_MFM_SYNC) {
                    pt++;
                }
                break;
            }
        }


        // We're done if this isn't a complete sector anymore
        if ((pt + ADF_MFM_SECTOR_SIZE/2) > pt_limit) {
            break;
        }


        // Pick up the sector
        FloppyDriver_DecodeSector(self, pt - pt_start, targetTrack);
        pt += ADF_MFM_SECTOR_SIZE/2;
    }

    
    // Validate the sector cache
    bool isGood = true;
    
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        if (self->sectorCache[i].state != kSectorState_Ok) {
            isGood = false;
            break;
        }
    }

    if (isGood) {
        self->scTrackNo = targetTrack;
        return EOK;
    }
    else {
        return EIO;
    }
}

static void FloppyDriver_EncodeSector(FloppyDriverRef _Nonnull self, struct ADF_MFMPhysicalSector* _Nonnull dma_buf, const CachedSector* _Nonnull s)
{
    uint32_t checksum;

    // Sync mark
    dma_buf->sync[0] = 0;
    dma_buf->sync[1] = 0;
    dma_buf->sync[2] = ADF_MFM_SYNC;
    dma_buf->sync[3] = ADF_MFM_SYNC;


    // Sector info
    mfm_encode_bits((const uint32_t*)&s->info, &dma_buf->payload.info.odd_bits, 1);


    // Sector label
    mfm_encode_bits(s->label, dma_buf->payload.label.odd_bits, 4);


    // Header checksum
    checksum = mfm_checksum(&dma_buf->payload.info.odd_bits, 10);
    mfm_encode_bits(&checksum, &dma_buf->payload.header_checksum.odd_bits, 1);


    // Data and data checksum. Note that we generate an incorrect data checksum
    // if the this sector is supposed to be a 'defective' sector. Aka a sector
    // that was originally stored on the disk and where the data checksum didn't
    // check out when we read it in. We do this to ensure that we do not
    // accidentally 'resurrect' a defective sector. We want to make sure that it
    // stays defective after we write it back to disk again.
    const size_t nLongs = ADF_SECTOR_DATA_SIZE / sizeof(uint32_t);

    mfm_encode_bits((const uint32_t*)s->data, dma_buf->payload.data.odd_bits, nLongs);

    checksum = (s->state == kSectorState_Ok) ? mfm_checksum(dma_buf->payload.data.odd_bits, 2 * nLongs) : 0;
    mfm_encode_bits(&checksum, &dma_buf->payload.data_checksum.odd_bits, 1);
}

// Encodes the currently cached track and stores the result in the DMA buffer.
static void FloppyDriver_EncodeTrack(FloppyDriverRef _Nonnull self)
{
    assert(self->scTrackNo != -1);

    // Track gap (1660 bytes)
    memset(self->dmaBuffer, 0xAA, ADF_GAP_SIZE);


    // Sector #0, ... Sector #10
    ADF_MFMPhysicalSector* pt = (ADF_MFMPhysicalSector*)&self->dmaBuffer[ADF_GAP_SIZE/2];
    const CachedSector* pcs = &self->sectorCache[0];

    for (int i = 0; i < self->sectorsPerTrack; i++) {
        FloppyDriver_EncodeSector(self, pt, pcs);
        pt++;
        pcs++;
    }


    // Extra words to work around the DMA write bug (dropping 3 last bits)
    pt->sync[0] = ADF_MFM_PRESYNC;
    pt->sync[1] = ADF_MFM_PRESYNC;


    // Adjust the MFM clock bits in the header and data portions of every sector
    // to make them compliant with the MFM spec. Note that we do this for the
    // 1080 bytes of the sector + the word following the sector. The reason is
    // that bit #0 of the last word in the sector data region may be 1 or 0 and
    // depending on that, the MSB in the word following the sector has to be
    // adjusted. So this word may come out as 0xAAAA or 0x2AAA.
    pt = (ADF_MFMPhysicalSector*)&self->dmaBuffer[ADF_GAP_SIZE/2];

    pt->sync[0] = ADF_MFM_PRESYNC;
    pt->sync[1] = ADF_MFM_PRESYNC;
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        mfm_adj_clock_bits((uint16_t*)&pt->payload, (ADF_MFM_SECTOR_SIZE + ADF_MFM_SYNC_SIZE/2) / 2);
        pt++;
    }
}

static errno_t FloppyDriver_EnsureTrackBuffered(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs)
{
    decl_try_err();
    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);

    if (self->scTrackNo == targetTrack) {
        return EOK;
    }

    err = FloppyDriver_PrepareIO(self, chs);
    if (err == EOK) {
        for (int8_t retry = 0; retry < self->params->retryCount; retry++) {
            err = FloppyDriver_DoSyncIO(self, false);
            if (err == EOK) {
                err = FloppyDriver_DecodeTrack(self, targetTrack);
            }
            
            if (err == EOK || err != EIO) {
                // Eg OK, disk changed, drive hardware lost
                break;
            }

            self->readErrorCount++;
        }
    }

    return err;
}

errno_t FloppyDriver_getSector(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    const errno_t err = FloppyDriver_EnsureTrackBuffered(self, chs);

    if (err == EOK) {
        const CachedSector* pcs = &self->sectorCache[chs->s];

        memcpy(data, &pcs->data[0], ADF_SECTOR_DATA_SIZE);
    }

    return FloppyDriver_FinalizeIO(self, err);
}

errno_t FloppyDriver_putSector(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t secSize)
{
    decl_try_err();
    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);

    try(FloppyDriver_EnsureTrackBuffered(self, chs));


    CachedSector* pcs = &self->sectorCache[chs->s];
    memcpy(pcs->data, data, ADF_SECTOR_DATA_SIZE);


    FloppyDriver_EncodeTrack(self);


    err = FloppyDriver_PrepareIO(self, chs);
    if (err == EOK) {
        err = FloppyDriver_DoSyncIO(self, true);
    }

catch:
    return FloppyDriver_FinalizeIO(self, err);
}


////////////////////////////////////////////////////////////////////////////////
// Formatting
////////////////////////////////////////////////////////////////////////////////

errno_t FloppyDriver_formatTrack(FloppyDriverRef _Nonnull self, const chs_t* chs, const void* _Nullable trackData, size_t secSize)
{
    decl_try_err();
    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);
    const int sectorCount = self->sectorsPerTrack;
    const uint8_t* src = trackData;
    CachedSector* pcs = self->sectorCache;

    for (int i = 0; i < sectorCount; i++) {
        pcs->state = kSectorState_Ok;
        pcs->info.format = ADF_FORMAT_V1;
        pcs->info.track = targetTrack;
        pcs->info.sector = i;
        pcs->info.sectors_until_gap = sectorCount - i;
        pcs->label[0] = 0;
        pcs->label[1] = 0;
        pcs->label[2] = 0;
        pcs->label[3] = 0;

        if (src) {
            memcpy(pcs->data, src, ADF_SECTOR_DATA_SIZE);
            src += ADF_SECTOR_DATA_SIZE;
        }
        else {
            memset(pcs->data, 0, ADF_SECTOR_DATA_SIZE);
        }

        pcs++;
    }
    self->scTrackNo = targetTrack;


    FloppyDriver_EncodeTrack(self);


    err = FloppyDriver_PrepareIO(self, chs);
    if (err == EOK) {
        err = FloppyDriver_DoSyncIO(self, true);
    }

    return FloppyDriver_FinalizeIO(self, err);
}


class_func_defs(FloppyDriver, DiskDriver,
override_func_def(deinit, FloppyDriver, Object)
override_func_def(doSenseDisk, FloppyDriver, DiskDriver)
override_func_def(onStart, FloppyDriver, Driver)
override_func_def(getSector, FloppyDriver, DiskDriver)
override_func_def(putSector, FloppyDriver, DiskDriver)
override_func_def(formatTrack, FloppyDriver, DiskDriver)
);
