//
//  AFDDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AFDDevicePriv.h"
#include <assert.h>
#include <string.h>
#include <driver/IOLib.h>
#include <ext/math.h>
#include <ext/nanotime.h>
#include <hal/clock.h>
#include <handler/IODiskHandler.h>
#include <kern/kalloc.h>
#include <kern/log.h>
#include <sched/vcpu.h>
#include "mfm.h"

IOCATS_DEF(g_cats, IODISK_FLOPPY);


// Allocates a floppy disk object. The object is set up to manage the physical
// floppy drive 'drive'.
errno_t AFDDevice_Create(int drive, DriveState ds, const DriveParams* _Nonnull params, AFDDeviceRef _Nullable * _Nonnull pOutDisk)
{
    decl_try_err();
    AFDDeviceRef self;
    drive_info_t dinf;

    dinf.platter = (params->driveType == kDriveType_3_5) ? DISK_DIAM_3_5 : DISK_DIAM_5_25;
    dinf.flags = 0;
    try(DiskDriver_Create(class(AFDDevice), 0, g_cats, &dinf, (IODriverRef*)&self));

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

catch:
    *pOutDisk = self;
    return err;
}

static void AFDDevice_deinit(AFDDeviceRef _Nonnull self)
{
    AFDDevice_MotorOff(self);
    kdispatch_cancel(DiskDriver_GetDispatchQueue(self), 0, (kdispatch_item_func_t)AFDDevice_CheckDiskChange, self);

    kfree(self->dmaBuffer);
    self->dmaBuffer = NULL;

    kfree(self->trackBuffer);
    self->trackBuffer = NULL;
}

int AFDDevice_getBootPriority(AFDDeviceRef _Nonnull self)
{
    return 1000 - self->drive;
}

static void _AFDDevice_doSenseDisk(AFDDeviceRef _Nonnull self)
{
    bool hasPhysDiskChange = false;

    self->tbTrackNo = -1;


    if ((AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState) & kDriveStatus_DiskChanged) != 0) {
        AFDDevice_ResetDriveDiskChange(self);
        hasPhysDiskChange = true;
    }


    if (hasPhysDiskChange || DiskDriver_IsDiskChangePending(self)) {
        const uint8_t status = AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState);
        const unsigned int hasDisk = ((status & kDriveStatus_DiskChanged) == 0) ? 1 : 0;

        if (hasDisk) {
            SensedDisk info;
    
            info.flags = DISK_FLAG_REMOVABLE;
            if ((status & kDriveStatus_IsReadOnly) == kDriveStatus_IsReadOnly) {
                info.flags |= DISK_FLAG_READ_ONLY;
            }
            info.sectorSize = ADF_SECTOR_DATA_SIZE;
            info.heads = ADF_HEADS_PER_CYL;
            info.cylinders = self->params->cylinders;
            info.sectorsPerTrack = self->sectorsPerTrack;
            info.sectorsPerRdwr = self->sectorsPerTrack;
            DiskDriver_NoteSensedDisk((DiskDriverRef)self, &info);
        }
        else {
            DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
        }
        AFDDevice_SetDiskChangeCounter(self);
    }
}

void AFDDevice_doSenseDisk(AFDDeviceRef _Nonnull self, IOSenseDiskCommand* _Nonnull req)
{
    _AFDDevice_doSenseDisk(self);
}

static void AFDDevice_Reset(AFDDeviceRef _Nonnull self)
{
    decl_try_err();
    nanotime_t interval;

    nanotime_from_ms(&interval, 800);

    // XXX hardcoded to DD for now
    self->sectorsPerTrack = ADF_DD_SECS_PER_TRACK;

    self->dmaReadWordCount = DMA_BYTE_SIZE(self->sectorsPerTrack) / 2;
    self->dmaWriteWordCount = self->dmaReadWordCount + ADF_MFM_SYNC_SIZE/2; /* +2 for the 3bit loss on write DMA bug*/
    assert(kalloc_options(sizeof(uint16_t) * self->dmaWriteWordCount, KALLOC_OPTION_UNIFIED, (void**) &self->dmaBuffer) == EOK);

    self->tbTrackNo = -1;
    assert(kalloc_options(TRACK_BUFFER_BYTE_SIZE(self->sectorsPerTrack), 0, (void**) &self->trackBuffer) == EOK);

    self->head = -1;
    self->cylinder = -1;


    // Move the head to track 0 so that we know where the head is and figure out
    // whether we're actually able to talk to the hardware successfully.
    err = AFDDevice_SeekToTrack_0(self);
    if (err == EOK) {
        self->flags.isOnline = 1;
        _AFDDevice_doSenseDisk(self);
        kdispatch_repeating(DiskDriver_GetDispatchQueue(self), 0, &NANOTIME_ZERO, &interval, (kdispatch_async_func_t)AFDDevice_CheckDiskChange, self);
    }
    else {
        self->flags.isOnline = 0;
        AFDDevice_OnHardwareLost(self);
    }
}

errno_t AFDDevice_start(AFDDeviceRef _Nonnull self)
{
    return kdispatch_async(DiskDriver_GetDispatchQueue(self), (kdispatch_async_func_t)AFDDevice_Reset, self);
}

errno_t AFDDevice_getDFSInfo(AFDDeviceRef _Nonnull self, IODFSInfo* _Nonnull info)
{
    info->name[0] = 'f';
    info->name[1] = 'd';
    info->name[2] = '0' + self->drive;
    info->name[3] = '\0';

    info->func = IODiskHandler_Create;
    info->uid = UID_ROOT;
    info->gid = GID_ROOT;
    info->perms = fs_perms_from_octal(0666);

    return EOK;
}

// Called when we've detected a loss of the drive hardware
static void AFDDevice_OnHardwareLost(AFDDeviceRef _Nonnull self)
{
    AFDDevice_MotorOff(self);
    kdispatch_cancel(DiskDriver_GetDispatchQueue(self), 0, (kdispatch_item_func_t)AFDDevice_CheckDiskChange, self);
    self->tbTrackNo = -1;
    DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
    self->flags.isOnline = 0;
}

static void AFDDevice_CheckDiskChange(AFDDeviceRef _Nonnull self)
{
    if (DiskDriver_IsDiskChangePending(self)) {
        return;
    }

    self->flags.dkCount--;
    if (self->flags.dkCount == 0) {
        self->flags.dkCount = self->flags.dkCountMax;

        if ((AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState) & kDriveStatus_DiskChanged) != 0) {
            AFDDevice_ResetDriveDiskChange(self);

            const uint8_t status = AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState);
            const unsigned int hasDisk = ((status & kDriveStatus_DiskChanged) == 0) ? 1 : 0;

            if (hasDisk) {
                DiskDriver_NoteDiskChanged((DiskDriverRef)self);
            }
            else {
                DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
            }
            AFDDevice_SetDiskChangeCounter(self);
        }
    }
}

static void AFDDevice_SetDiskChangeCounter(AFDDeviceRef _Nonnull self)
{
    self->flags.dkCountMax = (DiskDriver_HasDisk(self)) ? 1 : 5;
    self->flags.dkCount = self->flags.dkCountMax;
}


////////////////////////////////////////////////////////////////////////////////
// Motor Control
////////////////////////////////////////////////////////////////////////////////

static void AFDDevice_CancelDelayedMotorOff(AFDDeviceRef _Nonnull self)
{
    kdispatch_cancel(DiskDriver_GetDispatchQueue(self), 0, (kdispatch_item_func_t)AFDDevice_MotorOff, self);
}

// Turns the drive motor off.
static void AFDDevice_MotorOff(AFDDeviceRef _Nonnull self)
{
    // Note: may be called if the motor went off on us without our doing. We call
    // this function in this case to resync out software state with the hardware
    // state.
    if (self->flags.isOnline) {
        AFDBus_SetMotor(IODriver_GetProvider(self), &self->driveState, false);
    }
    self->flags.motorState = kMotor_Off;

    AFDDevice_CancelDelayedMotorOff(self);
}

// Turns the drive motor on and schedules an auto-motor-off in 4 seconds.
static void AFDDevice_MotorOn(AFDDeviceRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_Off) {
        AFDBus_SetMotor(IODriver_GetProvider(self), &self->driveState, true);
        self->flags.motorState = kMotor_SpinningUp;
    }

    AFDDevice_CancelDelayedMotorOff(self);
    
    nanotime_t dly; 
    nanotime_from_sec(&dly, 4);
    kdispatch_after(DiskDriver_GetDispatchQueue(self), 0, &dly, (kdispatch_async_func_t)AFDDevice_MotorOff, self);
}

// Waits until the drive is ready (motor is spinning at full speed). This function
// waits for at most 500ms for the disk to become ready.
// Returns EOK if the drive is ready; ETIMEDOUT if the drive failed to become
// ready in time.
static errno_t AFDDevice_WaitForDiskReady(AFDDeviceRef _Nonnull self)
{
    if (self->flags.motorState == kMotor_AtTargetSpeed) {
        return EOK;
    }
    else if (self->flags.motorState == kMotor_SpinningUp) {
        // Waits for at most 500ms for the motor to reach its target speed

        for (int i = 0; i < 50; i++) {
            const uint8_t status = AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState);

            if ((status & kDriveStatus_DiskChanged) == kDriveStatus_DiskChanged) {
                return EDISKCHANGE;
            }
            if ((status & kDriveStatus_DiskReady) == kDriveStatus_DiskReady) {
                self->flags.motorState = kMotor_AtTargetSpeed;
                return EOK;
            }

            IOSleep(10);
        }

        // Timed out. Turn the motor off for now so that another I/O request can
        // try spinning the motor up to its target speed again.
        AFDDevice_MotorOff(self);
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
static errno_t AFDDevice_SeekToTrack_0(AFDDeviceRef _Nonnull self)
{
    int steps = 0;

    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    // Since this is about resetting the drive we can't assume that we know whether
    // we have to wait 18ms or 2ms. So just wait for 18ms to be safe.
    IOSleep(18);
    
    while (true) {
        const uint8_t status = AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState);

        if ((status & kDriveStatus_AtTrack0) == kDriveStatus_AtTrack0) {
            break;
        }

        AFDBus_StepHead(IODriver_GetProvider(self), self->driveState, -1);
        
        steps++;
        if (steps > 80) {
            return ETIMEDOUT;
        }

        IOSleep(3);
    }
    AFDBus_SelectHead(IODriver_GetProvider(self), &self->driveState, 0);
    
    // Head settle time (includes the 100us settle time for the head select)
    IOSleep(15);
    
    self->head = 0;
    self->cylinder = 0;
    self->flags.wasMostRecentSeekInward = 0;

    return EOK;
}

// Seeks to the specified cylinder and selects the specified drive head.
// (0: outermost, 79: innermost, +: inward, -: outward).
static void AFDDevice_SeekTo(AFDDeviceRef _Nonnull self, int cylinder, int head)
{
    const int diff = cylinder - self->cylinder;
    const int cur_dir = (diff >= 0) ? 1 : -1;
    const int last_dir = (self->flags.wasMostRecentSeekInward) ? 1 : -1;
    const int nSteps = __abs(diff);
    const bool change_side = (self->head != head);
    
    // Wait 18 ms if we have to reverse the seek direction
    // Wait 2 ms if there was a write previously and we have to change the head
    const int seek_pre_wait_ms = (nSteps > 0 && cur_dir != last_dir) ? 18 : 0;
    const int side_pre_wait_ms = 2;
    const int pre_wait_ms = __max(seek_pre_wait_ms, side_pre_wait_ms);
    
    if (pre_wait_ms > 0) {
        IOSleep(pre_wait_ms);
    }
    
    
    // Seek if necessary
    if (nSteps > 0) {
        for (int i = nSteps; i > 0; i--) {
            AFDBus_StepHead(IODriver_GetProvider(self), self->driveState, cur_dir);     
            
            self->cylinder += cur_dir;
            
            if (cur_dir >= 0) {
                self->flags.wasMostRecentSeekInward = 1;
            } else {
                self->flags.wasMostRecentSeekInward = 0;
            }
            
            IOSleep(3);
        }
    }
    
    // Switch heads if necessary
    if (change_side) {
        AFDBus_SelectHead(IODriver_GetProvider(self), &self->driveState, head);
        self->head = head;
    }
    

    if (nSteps > 0) {
        // Seek settle time: 15ms.
        // We add 1ms in case we switched heads too.
        IOSleep(15 + 1);
    }
    else if (change_side) {
        // Head select settle time: 100us
        IODelay(100);
    }
}

static void AFDDevice_ResetDriveDiskChange(AFDDeviceRef _Nonnull self)
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

    AFDDevice_SeekTo(self, c, self->head);
    self->flags.shouldResetDiskChangeStepInward = !self->flags.shouldResetDiskChangeStepInward;
}


////////////////////////////////////////////////////////////////////////////////
// Disk I/O
////////////////////////////////////////////////////////////////////////////////

// Invoked at the beginning of a disk read/write operation to prepare the drive
// state. Ie turn motor on, seek, switch disk head, detect drive status, etc. 
static errno_t AFDDevice_PrepareIO(AFDDeviceRef _Nonnull self, const chs_t* _Nonnull chs)
{
    decl_try_err();

    // Make sure we still got the drive hardware and that the disk hasn't changed
    // on us
    if (!self->flags.isOnline) {
        return ENODEV;
    }


    // Make sure that the motor is turned on
    AFDDevice_MotorOn(self);


    // Seek to the required cylinder and select the required head
    if (self->cylinder != chs->c || self->head != chs->h) {
        AFDDevice_SeekTo(self, chs->c, chs->h);
    }


    // Wait until the motor has reached its target speed
    if (err == EOK) {
        err = AFDDevice_WaitForDiskReady(self);
    }

    return err;
}

// Invoked to do the actual read/write operation. Also validates that the disk
// hasn't been yanked out of the drive or changed on us while doing the I/O.
// Expects that the track buffer is properly prepared for the I/O.
static errno_t AFDDevice_DoSyncIO(AFDDeviceRef _Nonnull self, bool bWrite)
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

    return AFDBus_Dma(IODriver_GetProvider(self), self->driveState, precomp, self->dmaBuffer, nWords, bWrite);
}

// Invoked at the end of a disk I/O operation. Potentially translates the provided
// internal error code to an external one and kicks of disk-change related flow
// control and initiates a delayed motor-off operation.
static errno_t AFDDevice_FinalizeIO(AFDDeviceRef _Nonnull self, errno_t err)
{
    switch (err) {
        case EOK:
            break;

        case ETIMEDOUT:
            // A timeout may be caused by:
            // - no drive connected
            // - no disk in drive
            // - electro-mechanical problem
            AFDDevice_OnHardwareLost(self);
            err = ENODEV;
            break;

        case EDISKCHANGE:
            AFDDevice_MotorOff(self);
            AFDDevice_ResetDriveDiskChange(self);
            const uint8_t status = AFDBus_GetStatus(IODriver_GetProvider(self), self->driveState);
            if ((status & kDriveStatus_DiskChanged) == 0) {
                DiskDriver_NoteDiskChanged((DiskDriverRef)self);
            }
            else {
                DiskDriver_NoteSensedDisk((DiskDriverRef)self, NULL);
                err = ENOMEDIUM;
            }
            AFDDevice_SetDiskChangeCounter(self);
            break;

        default:
            err = EIO;
            break;
    }

    return err;
}


static void AFDDevice_DecodeSector(AFDDeviceRef _Nonnull self, int16_t offset, uint8_t targetTrack)
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
    ADF_Sector* ps = &self->trackBuffer[info.sector];
    int8_t* pst = &self->tbSectorState[info.sector];
    if (*pst != kSectorState_Missing) {
        *pst = kSectorState_NotUnique;
        return;
    }

    ps->info.format = info.format;
    ps->info.track = info.track;
    ps->info.sector = info.sector;
    ps->info.sectors_until_gap = info.sectors_until_gap;


    // Save the decoded label
    mfm_decode_bits(mfmSector->label.odd_bits, (uint32_t*)&ps->label[0], 4);


    // Save the decoded sector data
    mfm_decode_bits(mfmSector->data.odd_bits, (uint32_t*)&ps->data[0], ADF_SECTOR_DATA_SIZE / sizeof(uint32_t));


    // Validate the sector data
    mfm_decode_bits(&mfmSector->data_checksum.odd_bits, &diskChecksum, 1);
    myChecksum = mfm_checksum(mfmSector->data.odd_bits, 256);

    *pst = (diskChecksum == myChecksum) ? kSectorState_Ok : kSectorState_BadDataChecksum;
}

static errno_t AFDDevice_DecodeTrack(AFDDeviceRef _Nonnull self, uint8_t targetTrack)
{
    const uint16_t* pt = self->dmaBuffer;
    const uint16_t* pt_start = pt;
    const uint16_t* pt_limit = &self->dmaBuffer[self->dmaReadWordCount];

    // Invalidate the sector cache
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        self->tbSectorState[i] = kSectorState_Missing;
    }
    self->tbTrackNo = -1;


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
        AFDDevice_DecodeSector(self, pt - pt_start, targetTrack);
        pt += ADF_MFM_SECTOR_SIZE/2;
    }

    
    // Validate the sector cache
    bool isGood = true;
    
    for (int i = 0; i < self->sectorsPerTrack; i++) {
        if (self->tbSectorState[i] != kSectorState_Ok) {
            isGood = false;
            break;
        }
    }

    if (isGood) {
        self->tbTrackNo = targetTrack;
        return EOK;
    }
    else {
        return EIO;
    }
}

static void AFDDevice_EncodeSector(AFDDeviceRef _Nonnull self, struct ADF_MFMPhysicalSector* _Nonnull dma_buf, const ADF_Sector* _Nonnull s)
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

    checksum = (self->tbSectorState[s->info.sector] == kSectorState_Ok) ? mfm_checksum(dma_buf->payload.data.odd_bits, 2 * nLongs) : 0;
    mfm_encode_bits(&checksum, &dma_buf->payload.data_checksum.odd_bits, 1);
}

// Encodes the currently cached track and stores the result in the DMA buffer.
static void AFDDevice_EncodeTrack(AFDDeviceRef _Nonnull self)
{
    assert(self->tbTrackNo != -1);

    // Track gap (1660 bytes)
    memset(self->dmaBuffer, 0xAA, ADF_GAP_SIZE);


    // Sector #0, ... Sector #10
    ADF_MFMPhysicalSector* pt = (ADF_MFMPhysicalSector*)&self->dmaBuffer[ADF_GAP_SIZE/2];
    const ADF_Sector* ps = &self->trackBuffer[0];

    for (int i = 0; i < self->sectorsPerTrack; i++) {
        AFDDevice_EncodeSector(self, pt, ps);
        pt++;
        ps++;
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

static errno_t AFDDevice_EnsureTrackBuffered(AFDDeviceRef _Nonnull self, const chs_t* _Nonnull chs)
{
    decl_try_err();
    const uint8_t targetTrack = AFDDevice_TrackFromCylinderAndHead(chs);

    if (self->tbTrackNo == targetTrack) {
        return EOK;
    }

    err = AFDDevice_PrepareIO(self, chs);
    if (err == EOK) {
        for (int8_t retry = 0; retry < self->params->retryCount; retry++) {
            err = AFDDevice_DoSyncIO(self, false);
            if (err == EOK) {
                err = AFDDevice_DecodeTrack(self, targetTrack);
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

errno_t AFDDevice_getSector(AFDDeviceRef _Nonnull self, const chs_t* _Nonnull chs, uint8_t* _Nonnull data, size_t secSize)
{
    const errno_t err = AFDDevice_EnsureTrackBuffered(self, chs);

    if (err == EOK) {
        memcpy(data, self->trackBuffer[chs->s].data, ADF_SECTOR_DATA_SIZE);
    }

    return AFDDevice_FinalizeIO(self, err);
}

errno_t AFDDevice_putSector(AFDDeviceRef _Nonnull self, const chs_t* _Nonnull chs, const uint8_t* _Nonnull data, size_t secSize)
{
    decl_try_err();
    const uint8_t targetTrack = AFDDevice_TrackFromCylinderAndHead(chs);

    try(AFDDevice_EnsureTrackBuffered(self, chs));


    memcpy(self->trackBuffer[chs->s].data, data, ADF_SECTOR_DATA_SIZE);


    AFDDevice_EncodeTrack(self);


    err = AFDDevice_PrepareIO(self, chs);
    if (err == EOK) {
        err = AFDDevice_DoSyncIO(self, true);
    }

catch:
    return AFDDevice_FinalizeIO(self, err);
}


////////////////////////////////////////////////////////////////////////////////
// Formatting
////////////////////////////////////////////////////////////////////////////////

errno_t AFDDevice_doFormatTrack(AFDDeviceRef _Nonnull self, const chs_t* chs, char fillByte, size_t secSize)
{
    decl_try_err();
    const uint8_t targetTrack = AFDDevice_TrackFromCylinderAndHead(chs);
    const int sectorCount = self->sectorsPerTrack;
    ADF_Sector* ps = self->trackBuffer;
    int8_t* pst = self->tbSectorState;

    for (int i = 0; i < sectorCount; i++) {
        *pst = kSectorState_Ok;

        ps->info.format = ADF_FORMAT_V1;
        ps->info.track = targetTrack;
        ps->info.sector = i;
        ps->info.sectors_until_gap = sectorCount - i;
        ps->label[0] = 0;
        ps->label[1] = 0;
        ps->label[2] = 0;
        ps->label[3] = 0;

        memset(ps->data, fillByte, ADF_SECTOR_DATA_SIZE);

        ps++;
        pst++;
    }
    self->tbTrackNo = targetTrack;


    AFDDevice_EncodeTrack(self);


    err = AFDDevice_PrepareIO(self, chs);
    if (err == EOK) {
        err = AFDDevice_DoSyncIO(self, true);
    }

    return AFDDevice_FinalizeIO(self, err);
}


class_func_defs(AFDDevice, DiskDriver,
override_func_def(deinit, AFDDevice, Object)
override_func_def(getBootPriority, AFDDevice, DiskDriver)
override_func_def(start, AFDDevice, IODriver)
override_func_def(getDFSInfo, AFDDevice, IODriver)
override_func_def(getSector, AFDDevice, DiskDriver)
override_func_def(putSector, AFDDevice, DiskDriver)
override_func_def(doFormatTrack, AFDDevice, DiskDriver)
override_func_def(doSenseDisk, AFDDevice, DiskDriver)
);
