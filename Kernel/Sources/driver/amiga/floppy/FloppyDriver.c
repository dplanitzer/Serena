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

//#define TRACE_STATE 1
#ifdef TRACE_STATE
#define LOG(__drive, fmt, ...) if (self->drive == (__drive)) { printf(fmt, __VA_ARGS__); }
#else
#define LOG(drive, fmt, ...)
#endif


static const uint8_t gZeroSectorPayload[ADF_SECTOR_DATA_SIZE];


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

    self->tbCylinder = -1;
    self->tbHead = -1;

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

    kfree(self->trackBuffer);
    self->trackBuffer = NULL;

    kfree(self->trackCompositionBuffer);
    self->trackCompositionBuffer = NULL;
}

static void _FloppyDriver_doSenseDisk(FloppyDriverRef _Nonnull self)
{
    decl_try_err();
    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    bool didStep;
    bool hasDiskChangeDetected = false;

    if (!self->flags.didResetDrive) {
        self->flags.didResetDrive = 1;

        FloppyDriver_InvalidateTrackBuffer(self);
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
            FloppyDriver_InvalidateTrackBuffer(self);
            DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
        }
    }

    LOG(0, "%d: online: %d, has disk: %d\n", self->drive, (int)self->flags.isOnline, (int)DiskDriver_HasDisk(self));
}

void FloppyDriver_doSenseDisk(FloppyDriverRef _Nonnull self, SenseDiskRequest* _Nonnull req)
{
    _FloppyDriver_doSenseDisk(self);
}

static void FloppyDriver_Reset(FloppyDriverRef _Nonnull self)
{
    // XXX hardcoded to DD for now
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;

    self->trackReadWordCount = ADF_TRACK_READ_SIZE(self->sectorsPerTrack) / 2;
    self->trackWriteWordCount = ADF_TRACK_WRITE_SIZE(self->sectorsPerTrack) / 2;
    self->tbCylinder = -1;
    self->tbHead = -1;

    self->head = -1;
    self->cylinder = -1;

    FloppyDriver_InvalidateTrackBuffer(self);
    assert(kalloc_options(sizeof(uint16_t) * self->trackReadWordCount + 2 /* +2 for the 3bit loss on write DMA bug*/, KALLOC_OPTION_UNIFIED, (void**) &self->trackBuffer) == EOK);

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
    FloppyDriver_InvalidateTrackBuffer(self);
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
    self->flags.isOnline = 0;
}


////////////////////////////////////////////////////////////////////////////////
// Track Buffer
////////////////////////////////////////////////////////////////////////////////

static void FloppyDriver_InvalidateTrackBuffer(FloppyDriverRef _Nonnull self)
{
    self->tbCylinder = -1;
    self->tbHead = -1;

    memset(self->sectors, 0, sizeof(ADFSector) * self->sectorsPerTrack);
}


////////////////////////////////////////////////////////////////////////////////
// Track Composition Buffer
////////////////////////////////////////////////////////////////////////////////

static errno_t FloppyDriver_EnsureTrackCompositionBuffer(FloppyDriverRef _Nonnull self)
{
    if (self->trackCompositionBuffer) {
        return EOK;
    }
    else {
        return kalloc_options(sizeof(uint16_t) * self->trackWriteWordCount, 0, (void**) &self->trackCompositionBuffer);
    }
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
static errno_t FloppyDriver_DoSyncIO(FloppyDriverRef _Nonnull self, int16_t nWords, bool bWrite)
{
    uint16_t precomp;

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
    }
    else {
        precomp = kPrecompensation_0ns;
    }

    FloppyControllerRef fdc = FloppyDriver_GetController(self);
    return FloppyController_Dma(fdc, self->driveState, precomp, self->trackBuffer, nWords, bWrite);
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


static bool FloppyDriver_ScanSector(FloppyDriverRef _Nonnull self, int16_t offset, uint8_t targetTrack, int* _Nonnull pOutSectorsUntilGap)
{
    const ADF_MFMSector* mfmSector = (const ADF_MFMSector*)&self->trackBuffer[offset];
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
        return false;
    }


    // MFM decode the sector info long word
    mfm_decode_bits(&mfmSector->info.odd_bits, (uint32_t*)&info, 1);


    // Validate the sector info
    if (info.format == ADF_FORMAT_V1 
        && info.track == targetTrack
        && info.sector < self->sectorsPerTrack
        && info.sectors_until_gap <= self->sectorsPerTrack) {
        // Record the sector. Note that a sector may appear more than once because
        // we may have read more data from the disk than fits in a single track.
        // We keep the first occurrence of a sector and the second occurrence if
        // the first one has incorrect data and the second one's data is correct.
        ADFSector* s = &self->sectors[info.sector];

        // Validate the sector data
        mfm_decode_bits(&mfmSector->data_checksum.odd_bits, &diskChecksum, 1);
        myChecksum = mfm_checksum(mfmSector->data.odd_bits, 256);
        const bool diskDataValid = (diskChecksum == myChecksum) ? true : false;

        if (!s->isHeaderValid || (s->isHeaderValid && !s->isDataValid && diskDataValid)) {
            s->info = info;
            s->offsetToHeader = offset;
            s->isHeaderValid = true;

            s->isDataValid = diskDataValid;

            *pOutSectorsUntilGap = info.sectors_until_gap;
            return true;
        }
    }

    return false;
}

static errno_t FloppyDriver_ScanTrack(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs)
{
    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);
    const uint16_t* pt = self->trackBuffer;
    const uint16_t* pt_start = pt;
    const uint16_t* pt_limit = &self->trackBuffer[self->trackReadWordCount];
    int sectorsUntilGap = -1;
    int8_t nSectorsRead = 0;

    FloppyDriver_InvalidateTrackBuffer(self);

    while (pt < pt_limit && nSectorsRead < self->sectorsPerTrack) {
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
        if (!FloppyDriver_ScanSector(self, pt - pt_start, targetTrack, &sectorsUntilGap)) {
            return EIO;
        }
        pt += ADF_MFM_SECTOR_SIZE/2;
        nSectorsRead++;
    }

    self->tbCylinder = chs->c;
    self->tbHead = chs->h;

#if 0
    printf("c: %d, h: %d ---------- tb: %p, limit: %p\n", self->cylinder, self->head, self->trackBuffer, pt_limit);
    for(int i = 0; i < self->sectorsPerTrack; i++) {
        const ADFSector* s = &self->sectors[i];

        printf(" s: %*d, sug: %*d, off: %*d, v: %c%c, addr: %p\n",
            2, s->info.sector,
            2, s->info.sectors_until_gap,
            5, s->offsetToHeader*2,
            s->isHeaderValid ? 'H' : '-', s->isDataValid ? 'D' : '-',
            s);
    }
    printf("\n");
#endif

    return EOK;
}

static errno_t FloppyDriver_ReadTrack(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs)
{
    decl_try_err();
    
    FloppyDriver_InvalidateTrackBuffer(self);
    try(FloppyDriver_PrepareIO(self, chs));
    try(FloppyDriver_DoSyncIO(self, self->trackReadWordCount, false));
    try(FloppyDriver_ScanTrack(self, chs));
    try(FloppyDriver_FinalizeIO(self, err));

catch:
    return err;
}

errno_t FloppyDriver_getSector(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    decl_try_err();
    bool isCacheValid = (self->tbCylinder == chs->c && self->tbHead == chs->h) ? true : false;
    const ADFSector* ps = &self->sectors[chs->s];

    for (int8_t retry = 0; retry < self->params->retryCount; retry++) {
        if (!isCacheValid) {
            err = FloppyDriver_ReadTrack(self, chs);
        }

        if (err == EOK && ps->isDataValid) {
            // MFM decode the sector data
            const ADF_MFMSector* mfms = (const ADF_MFMSector*)&self->trackBuffer[ps->offsetToHeader];
            mfm_decode_bits((const uint32_t*)mfms->data.odd_bits, (uint32_t*)data, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
            break;
        }
        else if (!ps->isDataValid) {
            err = EIO;
        }

        self->readErrorCount++;
        isCacheValid = false;

        if (err != EIO) {
            break;
        }
    }

    return err;
}

errno_t FloppyDriver_putSector(FloppyDriverRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t secSize)
{
    decl_try_err();

    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);

    try(FloppyDriver_EnsureTrackCompositionBuffer(self));
    try(FloppyDriver_PrepareIO(self, chs));

    // Make sure that we got the right track in our track buffer.
    if (self->tbCylinder != chs->c || self->tbHead != chs->h) {
        for (int8_t retry = 0; retry < self->params->retryCount; retry++) {
            FloppyDriver_InvalidateTrackBuffer(self);
            err = FloppyDriver_DoSyncIO(self, self->trackReadWordCount, false);

            if (err == EOK) {
                if (FloppyDriver_ScanTrack(self, chs) != EOK) {
                    self->readErrorCount++;
                    err = EIO;
                }
            }
            if (err != EIO) {
                break;
            }
        }
    }
    if (err != EOK) {
        throw(err);
    }


    // Layout:
    // sector #1, ..., sector #11, gap
    ADF_MFMPhysicalSector* comp_buf = (ADF_MFMPhysicalSector*)self->trackCompositionBuffer;

    for (int i = 0; i < self->sectorsPerTrack; i++) {
        const void* s_dat;
        bool is_good;

        if (i != chs->s) {
            const ADFSector* s = &self->sectors[i];

            if (s->isHeaderValid) {
                const ADF_MFMSector* s_mfm_dat = (const ADF_MFMSector*)&self->trackBuffer[s->offsetToHeader];
                mfm_decode_bits((const uint32_t*)s_mfm_dat->data.odd_bits, (uint32_t*)self->sectorDataBuffer, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
                s_dat = self->sectorDataBuffer;
                is_good = s->isDataValid;
            }
            else {
                // A sector with a read error. Just fill the sector data with (raw) 0s
                memset(self->sectorDataBuffer, 0, ADF_SECTOR_DATA_SIZE);
                s_dat = self->sectorDataBuffer;
                is_good = false;
            }
        }
        else {
            s_dat = data;
            is_good = true;
        }

        mfm_make_sector(&comp_buf[i], targetTrack, i, self->sectorsPerTrack - i, s_dat, is_good);
    }


    // Override the start of the gap with a couple 0xAA (0) values. We do this
    // because the Amiga floppy controller hardware has a bug where it loses the
    // last 3 bits when writing to disk. Also, we want to minimize the chance
    // that the new gap may coincidentally contain the start (sync mark) of an
    // old sector.
    ADF_MFMPhysicalSector* tcb = (ADF_MFMPhysicalSector*)self->trackCompositionBuffer;
    memset(&tcb[self->sectorsPerTrack], 0, ADF_MFM_SYNC_SIZE);


    // Adjust the MFM clock bits in the header and data portions of every sector
    // to make them compliant with the MFM spec. Note that we do this for the
    // 1080 bytes of the sector + the word following the sector. The reason is
    // that bit #0 of the last word in the sector data region may be 1 or 0 and
    // depending on that, the MSB in the word following the sector has to be
    // adjusted. So this word may come out as 0xAAAA or 0x2AAA.  
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        const size_t trailerWordCount = (i < self->sectorsPerTrack - 1) ? 2 : ADF_MFM_SYNC_SIZE / 2;

        mfm_adj_clock_bits((uint16_t*)&tcb[i].payload, ADF_MFM_SECTOR_SIZE / 2 + trailerWordCount);
    }

    // First sector MFM encoded pre-sync words
    self->trackCompositionBuffer[0] = ADF_MFM_PRESYNC;
    self->trackCompositionBuffer[1] = ADF_MFM_PRESYNC;


    // Move the newly composed track to the DMA buffer
    memcpy(self->trackBuffer, self->trackCompositionBuffer, sizeof(uint16_t) * self->trackWriteWordCount);
    FloppyDriver_ScanTrack(self, chs);


    // Write the track back to disk
    try(FloppyDriver_DoSyncIO(self, self->trackWriteWordCount, true));

catch:
    return FloppyDriver_FinalizeIO(self, err);
}


////////////////////////////////////////////////////////////////////////////////
// Formatting
////////////////////////////////////////////////////////////////////////////////

errno_t FloppyDriver_formatTrack(FloppyDriverRef _Nonnull self, const chs_t* chs, const void* _Nullable trackData, size_t secSize)
{
    decl_try_err();
    ADF_MFMPhysicalSector* pt;
    const uint8_t targetTrack = FloppyDriver_TrackFromCylinderAndHead(chs);
    const uint8_t* sectorData = (trackData) ? trackData : gZeroSectorPayload;

    try(FloppyDriver_PrepareIO(self, chs));


    // gap, sector #1, ..., sector #11, extra word to work around DMA write bug
    FloppyDriver_InvalidateTrackBuffer(self);
    memset(self->trackBuffer, 0xAA, ADF_GAP_SIZE);
    pt = (ADF_MFMPhysicalSector*)&self->trackBuffer[ADF_GAP_SIZE/2];

    for (uint8_t sec = 0; sec < self->sectorsPerTrack; sec++) {
        mfm_make_sector(pt, targetTrack, sec, self->sectorsPerTrack - sec, sectorData, true);
        if (trackData) {
            sectorData += ADF_SECTOR_DATA_SIZE;
        }
        pt++;
    }
    pt->sync[0] = ADF_MFM_PRESYNC;
    pt->sync[1] = ADF_MFM_PRESYNC;



    // Adjust the MFM clock bits in the header and data portions of every sector
    // to make them compliant with the MFM spec. Note that we do this for the
    // 1080 bytes of the sector + the word following the sector. The reason is
    // that bit #0 of the last word in the sector data region may be 1 or 0 and
    // depending on that, the MSB in the word following the sector has to be
    // adjusted. So this word may come out as 0xAAAA or 0x2AAA.
    pt = (ADF_MFMPhysicalSector*)&self->trackBuffer[ADF_GAP_SIZE/2];

    pt->sync[0] = ADF_MFM_PRESYNC;
    pt->sync[1] = ADF_MFM_PRESYNC;
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        mfm_adj_clock_bits((uint16_t*)&pt->payload, (ADF_MFM_SECTOR_SIZE + ADF_MFM_SYNC_SIZE/2) / 2);
        pt++;
    }


    // Update the sector table and validate the track buffer
    FloppyDriver_ScanTrack(self, chs);


    // Write the track back to disk
    try(FloppyDriver_DoSyncIO(self, ADF_TRACK_FORMAT_SIZE(self->sectorsPerTrack) / 2, true));

catch:
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
