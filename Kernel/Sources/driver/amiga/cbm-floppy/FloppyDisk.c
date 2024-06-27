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
        const errno_t err0 = FloppyDisk_Create(i, fdc, &pOutDrives[i]);

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
    
    Lock_Init(&self->ioLock);
    try(DispatchQueue_Create(0, 1, kDispatchQoS_Interactive, 0, gVirtualProcessorPool, NULL, (DispatchQueueRef*)&self->dispatchQueue));

    self->fdc = pFdc;
    self->drive = drive;
    self->flags.isOnline = 0;
    self->flags.hasDisk = 0;

    Timer_Create(kTimeInterval_Zero, TimeInterval_MakeMilliseconds(2000), DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_OnOndiStateCheck, self), &self->ondiStateChecker);

    self->driveState = FloppyController_Reset(self->fdc, self->drive);

    if (FloppyController_GetDriveType(self->fdc, &self->driveState) == kDriveType_3_5) {
        FloppyDisk_ResetDrive(self);
    }

    FloppyDisk_ScheduleOndiStateChecker(self);
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
    FloppyDisk_CancelIdleWatcher(self);
    FloppyDisk_CancelOndiStateChecker(self);
    
    Timer_Destroy(self->ondiStateChecker);
    self->ondiStateChecker = NULL;

    FloppyDisk_DisposeTrackBuffer(self);
    self->fdc = NULL;

    Object_Release(self->dispatchQueue);
    self->dispatchQueue = NULL;

    Lock_Deinit(&self->ioLock);
}

// Resets the floppy drive. This function figures out whether there is an actual
// physical floppy drive connected and whether it responds to commands and it then
// moves the disk head to track #0.
// Note that this function leaves the floppy motor turned on and that it implicitly
// acknowledges any pending disk change.
// Upper layer code should treat this function like a disk change.
static void FloppyDisk_ResetDrive(FloppyDiskRef _Nonnull self)
{
    // XXX hardcoded to DD for now
    self->cylindersPerDisk = ADF_DD_CYLS_PER_DISK;
    self->headsPerCylinder = ADF_DD_HEADS_PER_CYL;
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;
    self->sectorsPerCylinder = self->headsPerCylinder * self->sectorsPerTrack;
    self->logicalBlockCapacity = self->sectorsPerCylinder * self->cylindersPerDisk;
    self->trackWordCountToRead = (self->sectorsPerTrack * (ADF_MFM_SYNC_SIZE + ADF_MFM_SECTOR_SIZE) + ADF_MFM_SECTOR_SIZE-2) / 2;
    self->trackWordCountToWrite = self->trackWordCountToRead;

    self->driveState = FloppyController_Reset(self->fdc, self->drive);
    
    self->flags.motorState = kMotor_Off;
    self->flags.isTrackBufferValid = 0;
    self->flags.wasMostRecentSeekInward = 0;
    self->flags.shouldResetDiskChangeStepInward = 0;
    self->head = -1;
    self->cylinder = -1;
    self->readErrorCount = 0;

    bool didStep;
    const errno_t err = FloppyDisk_SeekToTrack_0(self, &didStep);
    if (err == EOK) {
        if (!didStep) {
            FloppyDisk_ResetDriveDiskChange(self);
        }

        self->flags.isOnline = 1;
        self->flags.hasDisk = ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) == 0) ? 1 : 0;
    }
    else {
        self->flags.isOnline = 0;
        self->flags.hasDisk = 0;
    }
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
    err = kalloc_options(sizeof(uint16_t) * self->trackWordCountToRead, KALLOC_OPTION_UNIFIED, (void**) &self->trackBuffer);
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
}

// Turns the drive motor off.
static void FloppyDisk_MotorOff(FloppyDiskRef _Nonnull self)
{
    // Note: may be called if the motor when off on us without our doing. We call
    // this function in this case to resync out software state with the hardware
    // state.
    FloppyController_SetMotor(self->fdc, &self->driveState, false);
    self->flags.motorState = kMotor_Off;

    FloppyDisk_CancelIdleWatcher(self);
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
// Drive Flow Control
////////////////////////////////////////////////////////////////////////////////

// Called from a timer after the drive has been sitting idle for some time. Turn
// the drive motor off. 
static void FloppyDisk_OnIdle(FloppyDiskRef _Nonnull self)
{
    Lock_Lock(&self->ioLock);
    FloppyDisk_MotorOff(self);
    Lock_Unlock(&self->ioLock);
}

static void FloppyDisk_StartIdleWatcher(FloppyDiskRef _Nonnull self)
{
    FloppyDisk_CancelIdleWatcher(self);

    const TimeInterval curTime = MonotonicClock_GetCurrentTime();
    const TimeInterval deadline = TimeInterval_Add(curTime, TimeInterval_MakeMilliseconds(4000));
    Timer_Create(deadline, kTimeInterval_Zero, DispatchQueueClosure_Make((Closure1Arg_Func)FloppyDisk_OnIdle, self), &self->idleWatcher);
    if (self->idleWatcher) {
        DispatchQueue_DispatchTimer(self->dispatchQueue, self->idleWatcher);
    }
}

static void FloppyDisk_CancelIdleWatcher(FloppyDiskRef _Nonnull self)
{
    if (self->idleWatcher) {
        DispatchQueue_RemoveTimer(self->dispatchQueue, self->idleWatcher);
        Timer_Destroy(self->idleWatcher);
        self->idleWatcher = NULL;
    }
}

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

// Called from a timer as long as the drive is not connected or there's no disk
// in the drive. 
static void FloppyDisk_OnOndiStateCheck(FloppyDiskRef _Nonnull self)
{
    Lock_Lock(&self->ioLock);
    
    // Update the drive online state
    if (!self->flags.isOnline) {
        // Drive is offline
        if (FloppyController_GetDriveType(self->fdc, &self->driveState) == kDriveType_3_5) {
            self->flags.isOnline = 1;
            FloppyDisk_ResetDrive(self);
        }
    }
    else {
        // Drive is online
        // Only poll the drive type is the motor is off since the check forces
        // the motor off... 
        if (self->flags.motorState == kMotor_Off) {
            if (FloppyController_GetDriveType(self->fdc, &self->driveState) != kDriveType_3_5) {
                self->flags.isOnline = 0;
                self->flags.hasDisk = 0;
            }
        }

    }


    // Update the drive has-disk state
    if (self->flags.isOnline) {
        FloppyDisk_UpdateHasDiskState(self);
    }

    Lock_Unlock(&self->ioLock);
}

static void FloppyDisk_ScheduleOndiStateChecker(FloppyDiskRef _Nonnull self)
{
    if (!self->flags.isOndiStateCheckingActive) {
        self->flags.isOndiStateCheckingActive = 1;
        DispatchQueue_DispatchTimer(self->dispatchQueue, self->ondiStateChecker);
    }
}

static void FloppyDisk_CancelOndiStateChecker(FloppyDiskRef _Nonnull self)
{
    if (self->flags.isOndiStateCheckingActive) {
        self->flags.isOndiStateCheckingActive = 0;
        DispatchQueue_RemoveTimer(self->dispatchQueue, self->ondiStateChecker);
    }
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


////////////////////////////////////////////////////////////////////////////////
// Disk I/O
////////////////////////////////////////////////////////////////////////////////

static errno_t FloppyDisk_OnFailedIO(FloppyDiskRef _Nonnull self, errno_t err)
{
    FloppyDisk_MotorOff(self);

    if (err == ETIMEDOUT) {
        // A timeout may be caused by:
        // - no drive connected
        // - no disk in drive
        // - electro-mechanical problem
        self->flags.isOnline = 0;
        self->flags.hasDisk = 0;
        err = ENODEV;
    }
    else if (err == EDISKCHANGE) {
        FloppyDisk_UpdateHasDiskState(self);

        if (!self->flags.hasDisk) {
            err = ENOMEDIUM;
        }
    }
    else {
        err = EIO;
    }

    return err;
}

// Invoked at the beginning of a disk read/write operation to prepare the drive
// state. Ie turn motor on, seek, switch disk head, detect drive status, etc. 
static errno_t FloppyDisk_BeginIO(FloppyDiskRef _Nonnull self, int cylinder, int head)
{
    decl_try_err();

    // Make sure that the motor is turned on
    FloppyDisk_MotorOn(self);


    // Make sure that the disk hasn't changed on us
    if ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_DiskChanged) != 0) {
        throw(EDISKCHANGE);
    }


    // Seek to the required cylinder and select the required head
    if (self->cylinder != cylinder || self->head != head) {
        try(FloppyDisk_SeekTo(self, cylinder, head));
    }


    // Make sure we got a track buffer
    try(FloppyDisk_EnsureTrackBuffer(self));


    // Wait until the motor has reached its target speed
    try(FloppyDisk_WaitForDiskReady(self));

    return EOK;

catch:
    return FloppyDisk_OnFailedIO(self, err);
}

// Invoked at the end of a read/write operation to clean up and verify that the
// drive state is still okay
static errno_t FloppyDisk_EndIO(FloppyDiskRef _Nonnull self)
{
    decl_try_err();
    const uint8_t status = FloppyController_GetStatus(self->fdc, self->driveState);

    if ((status & kDriveStatus_DiskReady) == 0) {
        throw(ETIMEDOUT);
    }
    if ((status & kDriveStatus_DiskChanged) != 0) {
        throw(EDISKCHANGE);
    }

    // Instead of turning off the motor right away, let's wait some time and
    // turn the motor off if no further I/O request arrives in the meantime
    FloppyDisk_StartIdleWatcher(self);

    return EOK;

catch:
    return FloppyDisk_OnFailedIO(self, err);
}


static bool FloppyDisk_RegisterSector(FloppyDiskRef _Nonnull self, int16_t offset, int* _Nonnull pOutSectorsUntilGap)
{
    ADF_SectorInfo info;

    // MFM decode the sector info long word
    mfm_decode_sector((const uint32_t*)&self->trackBuffer[offset], (uint32_t*)&info, 1);


    // Validate the sector info
    if (info.format == ADF_FORMAT_V1 
        && info.track == (2*self->cylinder + self->head)
        && info.sector < self->sectorsPerTrack) {
        // Record the sector. Note that a sector may appear more than once because
        // we may have read more data from the disk than fits in a single track. We
        // keep the first occurrence of a sector.
        ADFSector* s = &self->sectors[info.sector];

        if ((s->flags & kSectorFlag_Exists) == 0) {
            // XXX validate the header checksum
            s->info = info;
            s->offsetToHeader = offset;
            s->flags = s->flags | kSectorFlag_Exists | kSectorFlag_IsValid;
            *pOutSectorsUntilGap = info.sectors_until_gap;
            return true;
        }
    }

    return false;
}

static void FloppyDisk_SectorizeTrackBuffer(FloppyDiskRef _Nonnull self)
{
    // Reset the sector table
    memset(self->sectors, 0, sizeof(ADFSector) * self->sectorsPerTrack);


    // Build the sector table
    const uint16_t* pt = self->trackBuffer;
    const uint16_t* pt_start = pt;
    const uint16_t* pt_limit = &self->trackBuffer[self->trackWordCountToRead];
    const uint16_t* pg_start = NULL;
    const uint16_t* pg_end = NULL;
    int sectorsUntilGap = -1;
    int valid_sector_count = 0;
    ADF_SectorInfo info;

   // print("start: %p, limit: %p\n", pt, pt_limit);
    while (pt < pt_limit && valid_sector_count < self->sectorsPerTrack) {
        const uint16_t* ps_start = pt;

        // Find the next MFM sync mark
        while (pt < pt_limit && *pt != ADF_MFM_SYNC) {
            pt++;
        }
        pt++;

        if (pt < pt_limit && *pt == ADF_MFM_SYNC) {
            pt++;
        }


        // Pick up the sector gap
        if (sectorsUntilGap == 1 && pg_start == NULL) {
            pg_start = ps_start;
            pg_end = pt - ADF_MFM_SYNC_SIZE/2;
        }


        // We're done if this isn't a complete sector anymore
        if (pt + ADF_MFM_SECTOR_SIZE/2 > pt_limit) {
            break;
        }


        // Pick up the sector
        FloppyDisk_RegisterSector(self, pt - pt_start, &sectorsUntilGap);
        pt += ADF_MFM_SECTOR_SIZE/2;
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

static errno_t FloppyDisk_ReadTrack(FloppyDiskRef _Nonnull self, int head, int cylinder)
{
    decl_try_err();
    
    if (self->cylinder == cylinder && self->head == head && self->flags.isTrackBufferValid) {
        return EOK;
    }

    try(FloppyDisk_BeginIO(self, cylinder, head));
    try(FloppyController_DoIO(self->fdc, self->driveState, self->trackBuffer, self->trackWordCountToRead, false));
    
    self->flags.isTrackBufferValid = 1;
    FloppyDisk_SectorizeTrackBuffer(self);
    
    try(FloppyDisk_EndIO(self));

    return EOK;

catch:
    self->flags.isTrackBufferValid = 0;
    return err;
}

static errno_t FloppyDisk_ReadSector(FloppyDiskRef _Nonnull self, int head, int cylinder, int sector, void* _Nonnull pBuffer)
{
    decl_try_err();
    
    // Read the track
    try(FloppyDisk_ReadTrack(self, head, cylinder));
    
    
    // Get the sector
    ADFSector* s = &self->sectors[sector];
    if ((s->flags & (kSectorFlag_Exists|kSectorFlag_IsValid) == 0) || !self->flags.isTrackBufferValid) {
        self->readErrorCount++;
        return EIO;
    }
    
    
    // MFM decode the sector data
    const ADF_MFMSector* mfms = (const ADF_MFMSector*)&self->trackBuffer[s->offsetToHeader];
    mfm_decode_sector((const uint32_t*)mfms->data.odd_bits, (uint32_t*)pBuffer, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
    return EOK;

catch:
    return err;
}

static errno_t FloppyDisk_WriteTrack(FloppyDiskRef _Nonnull self, int head, int cylinder)
{
    decl_try_err();
    
    // There must be a valid track cache
    assert(self->flags.isTrackBufferValid);
        
    try(FloppyDisk_BeginIO(self, cylinder, head));
    try(FloppyController_DoIO(self->fdc, self->driveState, self->trackBuffer, self->trackWordCountToWrite, true));
    try(FloppyDisk_EndIO(self));
    try(VirtualProcessor_Sleep(TimeInterval_MakeMicroseconds(1200)));

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
    ADFSector* s = &self->sectors[sector];
    if ((s->flags & (kSectorFlag_Exists|kSectorFlag_IsValid) == 0) || !self->flags.isTrackBufferValid) {
        return EIO;
    }
    
    
    // MFM encode the sector data
    ADF_MFMSector* mfms = (const ADF_MFMSector*)&self->trackBuffer[s->offsetToHeader];
    mfm_encode_sector((const uint32_t*)pBuffer, (uint32_t*)mfms->data.odd_bits, ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));
    
    
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


////////////////////////////////////////////////////////////////////////////////
// DiskDriver overrides
////////////////////////////////////////////////////////////////////////////////

bool FloppyDisk_HasDisk(FloppyDiskRef _Nonnull self)
{
    Lock_Lock(&self->ioLock);
    const bool r = (self->flags.isOnline && self->flags.hasDisk) ? true : false;
    Lock_Unlock(&self->ioLock);
    return r;
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
    return ((FloppyController_GetStatus(self->fdc, self->driveState) & kDriveStatus_IsReadOnly) == kDriveStatus_IsReadOnly) ? true : false;
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

    Lock_Lock(&self->ioLock);
//    print("lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);
    const errno_t err = FloppyDisk_ReadSector(self, h, c, s, pBuffer);
    Lock_Unlock(&self->ioLock);

    return err;
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

    Lock_Lock(&self->ioLock);
//    print("WRITE: lba: %d, c: %d, h: %d, s: %d\n", (int)lba, (int)c, (int)h, (int)s);
    const errno_t err = FloppyDisk_WriteSector(self, h, c, s, pBuffer);
    Lock_Unlock(&self->ioLock);

    return err;
}



class_func_defs(FloppyDisk, DiskDriver,
override_func_def(deinit, FloppyDisk, Object)
override_func_def(getBlockSize, FloppyDisk, DiskDriver)
override_func_def(getBlockCount, FloppyDisk, DiskDriver)
override_func_def(isReadOnly, FloppyDisk, DiskDriver)
override_func_def(getBlock, FloppyDisk, DiskDriver)
override_func_def(putBlock, FloppyDisk, DiskDriver)
);
