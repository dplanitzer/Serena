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

//#define TRACE_STATE 1
#ifdef TRACE_STATE
#define LOG(__drive, fmt, ...) if (self->drive == (__drive)) { print(fmt, __VA_ARGS__); }
#else
#define LOG(drive, fmt, ...)
#endif


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

    for (int i = 0; i < MAX_FLOPPY_DISK_DRIVES; i++) {
        DriveState ds = FloppyController_Reset(fdc, i);

        if (FloppyController_GetDriveType(fdc, &ds) == kDriveType_3_5) {
            const errno_t err0 = FloppyDisk_Create(i, ds, fdc, &pOutDrives[i]);
            
            if (err0 != EOK && nDrivesOkay == 0) {
                err = err0;
            }
        }
    }

    return (nDrivesOkay > 0) ? EOK : err;
}


////////////////////////////////////////////////////////////////////////////////



// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
static errno_t FloppyDisk_Create(int drive, DriveState ds, FloppyController* _Nonnull pFdc, FloppyDiskRef _Nullable * _Nonnull pOutDisk)
{
    decl_try_err();
    FloppyDisk* self;
    
    try(Object_Create(FloppyDisk, &self));
    
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Utility, kDispatchPriority_Normal, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue));

    self->fdc = pFdc;
    self->drive = drive;
    self->driveState = ds;

    // XXX hardcoded to DD for now
    self->cylindersPerDisk = ADF_DD_CYLS_PER_DISK;
    self->headsPerCylinder = ADF_DD_HEADS_PER_CYL;
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;
    self->sectorsPerCylinder = self->headsPerCylinder * self->sectorsPerTrack;
    self->blocksPerDisk = self->sectorsPerCylinder * self->cylindersPerDisk;
    self->trackReadWordCount = ADF_TRACK_READ_SIZE(self->sectorsPerTrack) / 2;
    self->trackWriteWordCount = ADF_TRACK_WRITE_SIZE(self->sectorsPerTrack) / 2;

    self->head = -1;
    self->cylinder = -1;
    self->readErrorCount = 0;

    self->flags.motorState = kMotor_Off;
    self->flags.isTrackBufferValid = 0;
    self->flags.wasMostRecentSeekInward = 0;
    self->flags.shouldResetDiskChangeStepInward = 0;
    self->flags.isOnline = 0;
    self->flags.hasDisk = 0;


    DispatchQueue_DispatchAsync(self->dispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_EstablishInitialDriveState, self));

    LOG(0, "%d: online: %d, has disk: %d\n", self->drive, (int)self->flags.isOnline, (int)self->flags.hasDisk);

    *pOutDisk = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutDisk = NULL;
    return err;
}

static void FloppyDisk_deinit(FloppyDiskRef _Nonnull self)
{
    if (self->dispatchQueue) {
        DispatchQueue_Terminate(self->dispatchQueue);
        DispatchQueue_WaitForTerminationCompleted(self->dispatchQueue);
        Object_Release(self->dispatchQueue);
        self->dispatchQueue = NULL;
    }

    FloppyDisk_CancelDelayedMotorOff(self);
    FloppyDisk_CancelUpdateHasDiskState(self);

    FloppyDisk_DisposeTrackBuffer(self);
    FloppyDisk_DisposeTrackCompositionBuffer(self);

    self->fdc = NULL;
}

// Establishes the base state for a newly discovered drive. This means that we
// move the disk head to track #0 and that we figure out whether a disk is loaded
// or not.
static void FloppyDisk_EstablishInitialDriveState(FloppyDiskRef _Nonnull self)
{
    bool didStep;
    const errno_t err = FloppyDisk_SeekToTrack_0(self, &didStep);
    if (err == EOK) {
        if (!didStep) {
            FloppyDisk_ResetDriveDiskChange(self);
        }

        self->flags.isOnline = 1;
        self->flags.hasDisk = ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) == 0) ? 1 : 0;

        if (!self->flags.hasDisk) {
            FloppyDisk_OnDiskRemoved(self);
        }
    }
    else {
        FloppyDisk_OnHardwareLost(self);
    }
}

// Called when we've detected that the disk has been removed from the drive
static void FloppyDisk_OnDiskRemoved(FloppyDiskRef _Nonnull self)
{
    FloppyDisk_ScheduleUpdateHasDiskState(self);
}

// Called when we've detected a loss of the drive hardware
static void FloppyDisk_OnHardwareLost(FloppyDiskRef _Nonnull self)
{
        self->flags.isOnline = 0;
        self->flags.hasDisk = 0;
}


////////////////////////////////////////////////////////////////////////////////
// Track Buffer
////////////////////////////////////////////////////////////////////////////////

static errno_t FloppyDisk_EnsureTrackBuffer(FloppyDiskRef _Nonnull self)
{
    decl_try_err();

    if (self->trackBuffer) {
        return EOK;
    }
    
    self->flags.isTrackBufferValid = 0;
    err = kalloc_options(sizeof(uint16_t) * self->trackReadWordCount, KALLOC_OPTION_UNIFIED, (void**) &self->trackBuffer);
    if (err == EOK) {
        err = kalloc_options(sizeof(ADFSector) * self->sectorsPerTrack, KALLOC_OPTION_CLEAR, (void**) &self->sectors);
        if (err == EOK) {
            return EOK;
        }
    }

    FloppyDisk_DisposeTrackBuffer(self);
    return err;
}

static void FloppyDisk_DisposeTrackBuffer(FloppyDiskRef _Nonnull self)
{
    if (self->trackBuffer) {
        kfree(self->trackBuffer);
        self->trackBuffer = NULL;
        self->flags.isTrackBufferValid = 0;
    }
    if (self->sectors) {
        kfree(self->sectors);
        self->sectors = NULL;
    }
}

static void FloppyDisk_ResetTrackBuffer(FloppyDiskRef _Nonnull self)
{
    // We go through all sectors in the track buffer and wipe out their sync words.
    // We do this to ensure that we won't accidentally pick up a sector from a
    // previous load operation if the DMA gets cut short and doesn't deliver a
    // full track for some reason
    for (int8_t i = 0; i < self->sectorsPerTrack; i++) {
        ADFSector* sector = &self->sectors[i];

        if ((sector->flags & kSectorFlag_IsValid) == kSectorFlag_IsValid) {
            uint16_t* bp = self->trackBuffer;
            uint16_t* dp = &bp[sector->offsetToHeader - 1];

            if (dp >= bp) *dp = 0;
            dp--;
            if (dp >= bp) *dp = 0;
        }

        sector->offsetToHeader = 0;
        sector->flags = 0;
    }

    self->flags.isTrackBufferValid = 0;
}


////////////////////////////////////////////////////////////////////////////////
// Track Composition Buffer
////////////////////////////////////////////////////////////////////////////////

static errno_t FloppyDisk_EnsureTrackCompositionBuffer(FloppyDiskRef _Nonnull self)
{
    if (self->trackCompositionBuffer == NULL) {
        return kalloc_options(sizeof(uint16_t) * self->trackWriteWordCount, 0, (void**) &self->trackCompositionBuffer);
    }
    else {
        return EOK;
    }
}

static void FloppyDisk_DisposeTrackCompositionBuffer(FloppyDiskRef _Nonnull self)
{
    if (self->trackCompositionBuffer) {
        kfree(self->trackCompositionBuffer);
        self->trackCompositionBuffer = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Motor Control
////////////////////////////////////////////////////////////////////////////////

// Turns the drive motor on and blocks the caller until the disk is ready.
static void FloppyDisk_MotorOn(FloppyDiskRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_Off) {
        FloppyController_SetMotor(self->fdc, &self->driveState, true);
        self->flags.motorState = kMotor_SpinningUp;
    }

    FloppyDisk_CancelDelayedMotorOff(self);
}

// Turns the drive motor off.
static void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull self)
{
    // Note: may be called if the motor when off on us without our doing. We call
    // this function in this case to resync out software state with the hardware
    // state.
    FloppyController_SetMotor(self->fdc, &self->driveState, false);
    self->flags.motorState = kMotor_Off;

    FloppyDisk_CancelDelayedMotorOff(self);
}

// Waits until the drive is ready (motor is spinning at full speed). This function
// waits for at most 500ms for the disk to become ready.
// Returns EOK if the drive is ready; ETIMEDOUT if the drive failed to become
// ready in time.
static errno_t FloppyDisk_WaitForDiskReady(FloppyDiskRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_AtTargetSpeed) {
        return EOK;
    }
    else if (self->flags.motorState == kMotor_SpinningUp) {
        // Waits for at most 500ms for the motor to reach its target speed
        const TimeInterval delay = TimeInterval_MakeMilliseconds(10);

        for (int i = 0; i < 50; i++) {
            const uint8_t status = FloppyController_GetStatus(self->fdc, self->driveState);

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
        FloppyDisk_MotorOff(self);
        return ETIMEDOUT;
    }
    else if (self->flags.motorState == kMotor_Off) {
        return EIO;
    }
}

// Called from a timer after the drive has been sitting idle for some time. Turn
// the drive motor off. 
static void FloppyDisk_OnDelayedMotorOff(FloppyDiskRef _Nonnull self)
{
    if (self->flags.isOnline) {
        FloppyDisk_MotorOff(self);
    }
}

static void FloppyDisk_DelayedMotorOff(FloppyDiskRef _Nonnull self)
{
    FloppyDisk_CancelDelayedMotorOff(self);

    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, TimeInterval_MakeSeconds(4));
    Timer_Create(deadline, kTimeInterval_Zero, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_OnDelayedMotorOff, self), &self->delayedMotorOff);
    if (self->delayedMotorOff) {
        DispatchQueue_DispatchTimer(self->dispatchQueue, self->delayedMotorOff);
    }
}

static void FloppyDisk_CancelDelayedMotorOff(FloppyDiskRef _Nonnull self)
{
    if (self->delayedMotorOff) {
        DispatchQueue_RemoveTimer(self->dispatchQueue, self->delayedMotorOff);
        Timer_Destroy(self->delayedMotorOff);
        self->delayedMotorOff = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Seeking & Head Selection
////////////////////////////////////////////////////////////////////////////////

// Seeks to track #0 and selects head #0. Returns ETIMEDOUT if the seek failed
// because there's probably no drive connected.
static errno_t FloppyDisk_SeekToTrack_0(FloppyDiskRef _Nonnull self, bool* _Nonnull pOutDidStep)
{
    decl_try_err();
    int steps = 0;

    self->flags.isTrackBufferValid = 0;
    *pOutDidStep = false;

    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(18)));
    
    while ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_AtTrack0) == 0) {
        FloppyController_StepHead(self->fdc, self->driveState, -1);
        
        steps++;
        if (steps > 80) {
            throw(ETIMEDOUT);
        }

        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(3)));
    }
    FloppyController_SelectHead(self->fdc, &self->driveState, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(15)));
    
    self->head = 0;
    self->cylinder = 0;
    self->flags.wasMostRecentSeekInward = 0;
    *pOutDidStep = (steps > 0) ? true : false;

catch:
    return err;
}

// Seeks to the specified cylinder and selects the specified drive head.
// (0: outermost, 79: innermost, +: inward, -: outward).
static errno_t FloppyDisk_SeekTo(FloppyDiskRef _Nonnull self, int cylinder, int head)
{
    decl_try_err();
    const int diff = cylinder - self->cylinder;
    const int cur_dir = (diff >= 0) ? 1 : -1;
    const int last_dir = (self->flags.wasMostRecentSeekInward) ? 1 : -1;
    const int nSteps = __abs(diff);
    const bool change_side = (self->head != head);

//    print("*** SeekTo(c: %d, h: %d)\n", cylinder, head);
    self->flags.isTrackBufferValid = 0;
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    const int seek_pre_wait_ms = (nSteps > 0 && cur_dir != last_dir) ? 18 : 0;
    const int side_pre_wait_ms = 2;
    const int pre_wait_ms = __max(seek_pre_wait_ms, side_pre_wait_ms);
    
    if (pre_wait_ms > 0) {
        try(VirtualProcessor_Sleep(TimeInterval_MakeMilliseconds(pre_wait_ms)));
    }
    
    
    // Seek if necessary
    if (nSteps > 0) {
        for (int i = nSteps; i > 0; i--) {
            FloppyController_StepHead(self->fdc, self->driveState, cur_dir);     
            
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
        FloppyController_SelectHead(self->fdc, &self->driveState, head);
        self->head = head;
    }
    
    // Seek settle time: 15ms
    // Head select settle time: 100us
    const int seek_settle_us = (nSteps > 0) ? 15*1000 : 0;
    const int side_settle_us = (change_side) ? 100 : 0;
    const int settle_us = __max(seek_settle_us, side_settle_us);
    
    if (settle_us > 0) {
        try(VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(settle_us)));
    }
    
catch:
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// Disk (Present) State
////////////////////////////////////////////////////////////////////////////////

static void FloppyDisk_ResetDriveDiskChange(FloppyDiskRef _Nonnull self)
{
    // We have to step the disk head to trigger a reset of the disk change bit.
    // We do this in a smart way in the sense that we step back and forth while
    // maintaining the general location of the disk head. Ie disk head is at
    // cylinder 3 and there's no disk in the drive. We step 4, 3, 4, 3... until
    // a disk is inserted.
    int c = (self->flags.shouldResetDiskChangeStepInward) ? self->cylinder + 1 : self->cylinder - 1;

    if (c > self->cylindersPerDisk - 1) {
        c = self->cylinder - 1;
        self->flags.shouldResetDiskChangeStepInward = 0;
    }
    else if (c < 0) {
        c = 1;
        self->flags.shouldResetDiskChangeStepInward = 1;
    }

    FloppyDisk_SeekTo(self, c, self->head);
    self->flags.shouldResetDiskChangeStepInward = !self->flags.shouldResetDiskChangeStepInward;
}

// Updates the drive's has-disk state
static void FloppyDisk_UpdateHasDiskState(FloppyDiskRef _Nonnull self)
{
    int newHasDisk = 0;

    if ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) != 0) {
        FloppyDisk_ResetDriveDiskChange(self);

        if ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) != 0) {
            newHasDisk = 0;
        }
        else {
            newHasDisk = 1;
        }
    }
    else {
        newHasDisk = self->flags.hasDisk;
    }


    self->flags.hasDisk = newHasDisk;

    LOG(0, "%d: online: %d, has disk: %d\n", self->drive, (int)self->flags.isOnline, (int)self->flags.hasDisk);
}

static void FloppyDisk_OnUpdateHasDiskStateCheck(FloppyDiskRef _Nonnull self)
{
    if (!self->flags.isOnline) {
        return;
    }

    FloppyDisk_UpdateHasDiskState(self);

    if (!self->flags.hasDisk) {
        FloppyDisk_ScheduleUpdateHasDiskState(self);
    }
}

static void FloppyDisk_ScheduleUpdateHasDiskState(FloppyDiskRef _Nonnull self)
{
    FloppyDisk_CancelUpdateHasDiskState(self);

    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, TimeInterval_MakeSeconds(3));
    Timer_Create(deadline, kTimeInterval_Zero, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_OnUpdateHasDiskStateCheck, self), &self->updateHasDiskState);
    if (self->updateHasDiskState) {
        DispatchQueue_DispatchTimer(self->dispatchQueue, self->updateHasDiskState);
    }
}

static void FloppyDisk_CancelUpdateHasDiskState(FloppyDiskRef _Nonnull self)
{
    if (self->updateHasDiskState) {
        DispatchQueue_RemoveTimer(self->dispatchQueue, self->updateHasDiskState);
        Timer_Destroy(self->updateHasDiskState);
        self->updateHasDiskState = NULL;
    }
}


////////////////////////////////////////////////////////////////////////////////
// Disk I/O
////////////////////////////////////////////////////////////////////////////////

// Invoked at the beginning of a disk read/write operation to prepare the drive
// state. Ie turn motor on, seek, switch disk head, detect drive status, etc. 
static errno_t FloppyDisk_BeginIO(FloppyDiskRef _Nonnull self, int cylinder, int head)
{
    decl_try_err();

    // Make sure we still got the drive hardware and that the disk hasn't changed
    // on us
    if (!self->flags.isOnline) {
        return ENODEV;
    }
    if ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) != 0) {
        throw(EDISKCHANGE);
    }


    // Make sure that the motor is turned on
    FloppyDisk_MotorOn(self);


    // Seek to the required cylinder and select the required head
    if (self->cylinder != cylinder || self->head != head) {
        try(FloppyDisk_SeekTo(self, cylinder, head));
    }


    // Make sure we got a track buffer
    try(FloppyDisk_EnsureTrackBuffer(self));


    // Wait until the motor has reached its target speed
    try(FloppyDisk_WaitForDiskReady(self));

catch:
    return err;
}

// Invoked to do the actual read/write operation. Also validates that the disk
// hasn't been yanked out of the drive or changed on us while doing the I/O.
// Expects that the track buffer is properly prepared for the I/O.
static errno_t FloppyDisk_DoIO(FloppyDiskRef _Nonnull self, bool bWrite)
{
    errno_t err = FloppyController_DoIO(self->fdc, self->driveState, self->trackBuffer, self->trackReadWordCount, bWrite);
    const uint8_t status = FloppyController_GetStatus(self->fdc, self->driveState);

    if ((status & kDriveStatus_DiskReady) == 0) {
        err = ETIMEDOUT;
    }
    if ((status & kDriveStatus_DiskChanged) != 0) {
        err = EDISKCHANGE;
    }
    if (!bWrite) {
        self->flags.isTrackBufferValid = (err == EOK) ? 1 : 0;
    }

    return err;
}

// Invoked at the end of a disk I/O operation. Potentially translates the provided
// internal error code to an external one and kicks of disk-change related flow
// control and initiates a delayed motor-off operation.
static errno_t FloppyDisk_EndIO(FloppyDiskRef _Nonnull self, errno_t err)
{
    switch (err) {
        case ETIMEDOUT:
            // A timeout may be caused by:
            // - no drive connected
            // - no disk in drive
            // - electro-mechanical problem
            FloppyDisk_OnHardwareLost(self);
            err = ENODEV;
            break;

        case EDISKCHANGE:
            FloppyDisk_UpdateHasDiskState(self);

            if (!self->flags.hasDisk) {
                FloppyDisk_OnDiskRemoved(self);
                FloppyDisk_MotorOff(self);
                err = ENOMEDIUM;
            }
            else {
                FloppyDisk_CancelUpdateHasDiskState(self);
            }
            break;

        case EOK:
            break;

        default:
            err = EIO;
            break;
    }

    if (!self->flags.isOnline && err != ENOMEDIUM) {
        // Instead of turning off the motor right away, let's wait some time and
        // turn the motor off if no further I/O request arrives in the meantime
        FloppyDisk_DelayedMotorOff(self);
    }

    return err;
}


static bool FloppyDisk_RecognizeSector(FloppyDiskRef _Nonnull self, int16_t offset, int* _Nonnull pOutSectorsUntilGap)
{
    const ADF_MFMSector* mfmSector = (const ADF_MFMSector*)&self->trackBuffer[offset];
    ADF_SectorInfo info;
    ADF_Checksum headerChecksum, dataChecksum, myChecksum;

    // Decode the stored sector header checksum, calculate our checksum and make
    // sure that they match. This is not a valid sector if they don't match.
    // The header checksum is calculated based on:
    // - 2 MFM info longwords
    // - 8 MFM sector label longwords
    mfm_decode_sector(&mfmSector->header_checksum.odd_bits, &headerChecksum, 1);
    myChecksum = mfm_checksum(&mfmSector->info.odd_bits, 2 + 8);

    if (headerChecksum != myChecksum) {
        return false;
    }


    // Validate the sector data
    mfm_decode_sector(&mfmSector->data_checksum.odd_bits, &dataChecksum, 1);
    myChecksum = mfm_checksum(mfmSector->data.odd_bits, 256);

    if (dataChecksum != myChecksum) {
        return false;
    }


    // MFM decode the sector info long word
    mfm_decode_sector(&mfmSector->info.odd_bits, (uint32_t*)&info, 1);


    // Validate the sector info
    if (info.format == ADF_FORMAT_V1 
        && info.track == (2*self->cylinder + self->head)
        && info.sector < self->sectorsPerTrack
        && info.sectors_until_gap <= self->sectorsPerTrack) {
        // Record the sector. Note that a sector may appear more than once because
        // we may have read more data from the disk than fits in a single track.
        // We keep the first occurrence of a sector.
        ADFSector* s = &self->sectors[info.sector];

        if ((s->flags & kSectorFlag_IsValid) == 0) {
            s->info = info;
            s->offsetToHeader = offset;
            s->flags |= kSectorFlag_IsValid;
            *pOutSectorsUntilGap = info.sectors_until_gap;
            return true;
        }
    }

    return false;
}

static void FloppyDisk_ScanTrack(FloppyDiskRef _Nonnull self)
{
    // Build the sector table
    const uint16_t* pt = self->trackBuffer;
    const uint16_t* pt_start = pt;
    const uint16_t* pt_limit = &self->trackBuffer[self->trackReadWordCount];
    const uint16_t* pg_start = NULL;
    const uint16_t* pg_end = NULL;
    int sectorsUntilGap = -1;
    int8_t nSectorsRead = 0;
    ADF_SectorInfo info;

   // print("start: %p, limit: %p\n", pt, pt_limit);
    while (pt < pt_limit && nSectorsRead < self->sectorsPerTrack) {

        // Find the next MFM sync mark
        while (pt < pt_limit && *pt != ADF_MFM_SYNC) {
            pt++;
        }


        // Pick up the end of the sector gap
        if (pg_start && pg_end == NULL) {
            pg_end = pt;
        }


        // Skip over the 2 sync words.
        pt++;
        if (pt < pt_limit && *pt == ADF_MFM_SYNC) {
            pt++;
        }


        // We're done if this isn't a complete sector anymore
        if ((pt + ADF_MFM_SECTOR_SIZE/2) > pt_limit) {
            break;
        }


        // Pick up the sector
        if (FloppyDisk_RecognizeSector(self, pt - pt_start, &sectorsUntilGap)) {
            nSectorsRead++;
        }
        pt += ADF_MFM_SECTOR_SIZE/2;


        // Pick up the start of the sector gap
        if (sectorsUntilGap == 1 && pg_start == NULL) {
            pg_start = pt;
        }
    }

    self->gapSize = (pg_start && pg_end) ? pg_end - pg_start : 0;

#if 0
    print("c: %d, h: %d ----------\n", self->cylinder, self->head);
    for(int i = 0; i < self->sectorsPerTrack; i++) {
        print(" s: %d, sug: %d, off: %d\n", self->sectors[i].info.sector, self->sectors[i].info.sectors_until_gap, self->sectors[i].offsetToHeader);
    }
    print(" gap at: %d, gap size: %d\n", pg_start - pt_start, self->gapSize);
    print("\n");
#endif
}

static errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, void* _Nonnull pBuffer)
{
    decl_try_err();
    
    // Make sure that we either already got the desired track cached or if not,
    // that we read it in
    if (!self->flags.isTrackBufferValid || self->cylinder != cylinder || self->head != head) {
        err = FloppyDisk_BeginIO(self, cylinder, head);

        if (err == EOK) {
            for (int retry = 0; retry < 4; retry++) {
                FloppyDisk_ResetTrackBuffer(self);

                err = FloppyDisk_DoIO(self, false);
                if (err == EOK) {
                    FloppyDisk_ScanTrack(self);

                    if ((self->sectors[sector].flags & kSectorFlag_IsValid) == 0) {
                        self->readErrorCount++;
                        err = EIO;
                    }
                }
                if (err != EIO) {
                    break;
                }
            }
        }

        err = FloppyDisk_EndIO(self, err);
    }
    
    if (err == EOK) {
        // MFM decode the sector data
        const ADF_MFMSector* mfms = (const ADF_MFMSector*)&self->trackBuffer[self->sectors[sector].offsetToHeader];
        mfm_decode_sector((const uint32_t*)mfms->data.odd_bits, (uint32_t*)pBuffer, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
    }
    else {
        self->flags.isTrackBufferValid = 0;
    }
    return err;
}

static void FloppyDisk_BuildSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, const BuildSectorSource* _Nonnull pSrc)
{
    ADF_MFMSyncedSector* pt = (ADF_MFMSyncedSector*)self->trackCompositionBuffer;
    ADF_MFMSyncedSector* dst = (ADF_MFMSyncedSector*)&pt[sector];
    ADF_SectorInfo info;
    uint32_t label[4] = {0,0,0,0};
    uint32_t checksum;

    // Sync marks
    dst->sync[0] = ADF_MFM_PRESYNC;
    dst->sync[1] = ADF_MFM_PRESYNC;
    dst->sync[2] = ADF_MFM_SYNC;
    dst->sync[3] = ADF_MFM_SYNC;


    // Sector info
    info.format = ADF_FORMAT_V1;
    info.track = 2*cylinder + head;
    info.sector = sector;
    info.sectors_until_gap = self->sectorsPerTrack - sector;
    mfm_encode_sector((const uint32_t*)&info, &dst->payload.info.odd_bits, 1);


    // Sector label
    mfm_encode_sector(label, dst->payload.label.odd_bits, 4);


    // Header checksum
    checksum = mfm_checksum(&dst->payload.info.odd_bits, 10);
    mfm_encode_sector(&checksum, &dst->payload.header_checksum.odd_bits, 1);


    // Data and data checksum
    if (pSrc->isEncoded) {
        memcpy(dst->payload.data.odd_bits, pSrc->u.encoded->data.odd_bits, sizeof(ADF_MFMData));

        dst->payload.data_checksum.odd_bits = pSrc->u.encoded->data_checksum.odd_bits;
        dst->payload.data_checksum.even_bits = pSrc->u.encoded->data_checksum.even_bits;
    }
    else {
        const int nLongs = ADF_SECTOR_DATA_SIZE / sizeof(uint32_t);

        mfm_encode_sector((const uint32_t*)pSrc->u.raw, dst->payload.data.odd_bits, nLongs);

        checksum = mfm_checksum(dst->payload.data.odd_bits, 2 * nLongs);
        mfm_encode_sector(&checksum, &dst->payload.data_checksum.odd_bits, 1);
    }
}

static errno_t FloppyDisk_WriteSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, const void* pBuffer)
{
    decl_try_err();

    try(FloppyDisk_BeginIO(self, cylinder, head));


    // Make sure that we got the whole track in the track buffer
    if (!self->flags.isTrackBufferValid) {
        for (int retry = 0; retry < 4; retry++) {
            FloppyDisk_ResetTrackBuffer(self);

            err = FloppyDisk_DoIO(self, false);
            if (err == EOK) {
                FloppyDisk_ScanTrack(self);
            }
            if (err != EIO) {
                break;
            }
        }
    }

    try(FloppyDisk_EnsureTrackCompositionBuffer(self));


    // Layout:
    // sector #1, ..., sector #11, gap
    BuildSectorSource sec_src;

    for (int i = 0; i < self->sectorsPerTrack; i++) {
        if (i != sector) {
            const ADFSector* s = &self->sectors[i];

            if ((s->flags & kSectorFlag_IsValid) == kSectorFlag_IsValid) {
                sec_src.isEncoded = true;
                sec_src.u.encoded = (const ADF_MFMSector*)&self->trackBuffer[s->offsetToHeader];
            }
            else {
                // A sector with a read error. Just put random data down
                sec_src.isEncoded = false;
                sec_src.u.raw = pBuffer;
            }

            FloppyDisk_BuildSector(self, head, cylinder, i, &sec_src);
        }
        else {
            sec_src.isEncoded = false;
            sec_src.u.raw = pBuffer;
            FloppyDisk_BuildSector(self, head, cylinder, sector, &sec_src);
        }
    }


    // Override the start of the gap with a couple 0xAA (0) values. We do this
    // because the Amiga floppy controller hardware has a bug where it loses the
    // last 3 bits when writing to disk. Also, we want to minimize the chance
    // that the new gap may coincidentally contain the start (sync mark) of a
    // sector.  
    memset(&self->trackCompositionBuffer[sizeof(ADF_MFMSyncedSector)/2 * self->sectorsPerTrack], 0xAA, ADF_MFM_SYNC_SIZE);


    // Move the newly composed track to the DMA buffer
    memcpy(self->trackBuffer, self->trackCompositionBuffer, sizeof(uint16_t) * self->trackWriteWordCount);
    memset(self->sectors, 0, sizeof(ADFSector) * self->sectorsPerTrack);
    FloppyDisk_ScanTrack(self);
    self->flags.isTrackBufferValid = 1;


    // Write the track back to disk
    try(FloppyController_DoIO(self->fdc, self->driveState, self->trackBuffer, self->trackWriteWordCount, true));
    VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(1200));

catch:
    return FloppyDisk_EndIO(self, err);
}


////////////////////////////////////////////////////////////////////////////////
// DiskDriver overrides
////////////////////////////////////////////////////////////////////////////////

bool FloppyDisk_HasDisk(FloppyDiskRef _Nonnull self)
{
    return (self->flags.isOnline && self->flags.hasDisk) ? true : false;
}

// Returns the size of a block.
size_t FloppyDisk_getBlockSize(FloppyDiskRef _Nonnull self)
{
    return ADF_SECTOR_DATA_SIZE;
}

// Returns the number of blocks that the disk is able to store.
LogicalBlockCount FloppyDisk_getBlockCount(FloppyDiskRef _Nonnull self)
{
    return self->blocksPerDisk;
}

// Returns true if the disk if read-only.
bool FloppyDisk_isReadOnly(FloppyDiskRef _Nonnull self)
{
    return ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_IsReadOnly) == kDriveStatus_IsReadOnly) ? true : false;
}

// Reads the contents of the block at index 'lba'. 'buffer' must be big
// enough to hold the data of a block. Blocks the caller until the read
// operation has completed. Note that this function will never return a
// partially read block. Either it succeeds and the full block data is
// returned, or it fails and no block data is returned.
static void FloppyDisk_ReadBlock(DiskRequest* _Nonnull req)
{
    FloppyDiskRef self = req->self;
    void* pBuffer = req->pBuffer;
    LogicalBlockAddress lba = req->lba;

    if (lba >= self->blocksPerDisk) {
        req->err = EIO;
        return;
    }

    const int c = lba / self->sectorsPerCylinder;
    const int h = (lba / self->sectorsPerTrack) % self->headsPerCylinder;
    const int s = lba % self->sectorsPerTrack;

//    print("lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);

    req->err = FloppyDisk_ReadSector(self, h, c, s, pBuffer);
}

errno_t FloppyDisk_getBlock(FloppyDiskRef _Nonnull self, void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    DiskRequest req;

    req.self = self;
    req.pBuffer = pBuffer;
    req.lba = lba;

    DispatchQueue_DispatchSync(self->dispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_ReadBlock, &req));
    return req.err;
}

// Writes the contents of 'pBuffer' to the block at index 'lba'. 'pBuffer'
// must be big enough to hold a full block. Blocks the caller until the
// write has completed. The contents of the block on disk is left in an
// indeterminate state of the write fails in the middle of the write. The
// block may contain a mix of old and new data.
static void FloppyDisk_WriteBlock(DiskRequest* _Nonnull req)
{
    FloppyDiskRef self = req->self;
    const void* pBuffer = req->pBuffer;
    LogicalBlockAddress lba = req->lba;

    if (lba >= self->blocksPerDisk) {
        req->err = EIO;
        return;
    }

    const int c = lba / self->sectorsPerCylinder;
    const int h = (lba / self->sectorsPerTrack) % self->headsPerCylinder;
    const int s = lba % self->sectorsPerTrack;

//    print("WRITE: lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);
    req->err = FloppyDisk_WriteSector(self, h, c, s, pBuffer);
}

errno_t FloppyDisk_putBlock(FloppyDiskRef _Nonnull self, const void* _Nonnull pBuffer, LogicalBlockAddress lba)
{
    DiskRequest req;

    req.self = self;
    req.pBuffer = (void*)pBuffer;
    req.lba = lba;

    DispatchQueue_DispatchSync(self->dispatchQueue, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_WriteBlock, &req));
    return req.err;
}



class_func_defs(FloppyDisk, DiskDriver,
override_func_def(deinit, FloppyDisk, Object)
override_func_def(getBlockSize, FloppyDisk, DiskDriver)
override_func_def(getBlockCount, FloppyDisk, DiskDriver)
override_func_def(isReadOnly, FloppyDisk, DiskDriver)
override_func_def(getBlock, FloppyDisk, DiskDriver)
override_func_def(putBlock, FloppyDisk, DiskDriver)
);
