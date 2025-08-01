//
//  FloppyController.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyDriverPriv.h"
#include "FloppyControllerPkg.h"
#include "adf.h"
#include <machine/clock.h>
#include <machine/InterruptController.h>
#include <machine/amiga/chipset.h>
#include <kern/timespec.h>
#include <sched/cnd.h>
#include <sched/delay.h>
#include <sched/mtx.h>
#include <sched/sem.h>
#include <sched/vcpu.h>


const DriveParams   kDriveParams_3_5 = {
    kDriveType_3_5,
    ADF_HEADS_PER_CYL,
    ADF_CYLS_PER_DISK,
    ADF_CYLS_PER_DISK/2,    // 0ns
    INT8_MAX,               // 140ns
    INT8_MAX,               // 280ns
    INT8_MAX,               // 560ns
    8,
};

const DriveParams   kDriveParams_5_25 = {
    kDriveType_3_5,
    2,
    40,
    20,                     // 0ns
    INT8_MAX,               // 140ns
    INT8_MAX,               // 280ns
    INT8_MAX,               // 560ns
    8,
};


#define MAX_FLOPPY_DISK_DRIVES  4

final_class_ivars(FloppyController, Driver,
    mtx_t               mtx;       // Used to ensure that we issue commands to the hardware atomically since all drives share the same CIA and DMA register set
    cnd_t               cv;
    sem_t               done;       // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
    struct __fdcFlags {
        unsigned int        inUse:1;
        unsigned int        reserved:31;
    }                   flags;
);


static void FloppyController_Destroy(FloppyControllerRef _Nullable self);
static void _FloppyController_SetMotor(FloppyControllerRef _Locked _Nonnull self, DriveState* _Nonnull cb, bool onoff);


// Creates the floppy controller
errno_t FloppyController_Create(DriverRef _Nullable parent, FloppyControllerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FloppyControllerRef self;
    
    try(Driver_Create(class(FloppyController), 0, parent, (DriverRef*)&self));

    mtx_init(&self->mtx);
    cnd_init(&self->cv);
    sem_init(&self->done, 0);
        
    try(InterruptController_AddSemaphoreInterruptHandler(gInterruptController,
                                                         INTERRUPT_ID_DISK_BLOCK,
                                                         INTERRUPT_HANDLER_PRIORITY_NORMAL,
                                                         &self->done,
                                                         &self->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController,
                                                       self->irqHandler,
                                                       true);
    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

// Destroys the floppy controller.
static void FloppyController_deinit(FloppyControllerRef _Nonnull self)
{
    if (self->irqHandler != 0) {
        try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
    }
    self->irqHandler = 0;
        
    sem_deinit(&self->done);
    cnd_deinit(&self->cv);
    mtx_deinit(&self->mtx);
}

static errno_t FloppyController_DetectDevices(FloppyControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    for (int i = 0; i < MAX_FLOPPY_DISK_DRIVES; i++) {
        DriveState ds = FloppyController_ResetDrive(self, i);
        const uint32_t dt = FloppyController_GetDriveType(self, &ds);
        const DriveParams* dp = NULL;
        FloppyDriverRef drive;
        
        switch (dt) {
            case kDriveType_3_5:    dp = &kDriveParams_3_5; break;
            case kDriveType_5_25:   dp = &kDriveParams_5_25; break;
            default:                break;
        }

        if (dp) {
            if ((err = FloppyDriver_Create((DriverRef)self, i, ds, dp, &drive)) == EOK) {
                err = Driver_StartAdoptChild((DriverRef)self, (DriverRef)drive);
                if (err != EOK) {
                    Object_Release(drive);
                }
            }
        }
    }

    return err;
}

errno_t FloppyController_onStart(FloppyControllerRef _Nonnull _Locked self)
{
    decl_try_err();

    BusEntry be;
    be.name = "fd-bus";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    DriverEntry de;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    try(Driver_PublishBus((DriverRef)self, &be, &de));

    
    // Discover as many floppy drives as possible. We ignore drives that generate
    // an error while trying to initialize them.
    err = FloppyController_DetectDevices(self);

catch:
    return err;
}

DriveState FloppyController_ResetDrive(FloppyControllerRef _Nonnull self, int drive)
{
    CIAB_BASE_DECL(ciab);
    uint8_t r;

    // motor off; all drives deselected; head 0; stepping off
    r = CIAB_PRBF_DSKMTR | CIAB_PRBF_DSKSELALL | CIAB_PRBF_DSKSTEP;
    
    // select 'drive'
    r &= ~(1 << (CIAB_PRBB_DSKSEL0 + (drive & 0x03)));

    // Make sure that the motor is off and then deselect the drive
    mtx_lock(&self->mtx);
    *CIA_REG_8(ciab, CIA_PRB) = r;
    delay_us(1);
    *CIA_REG_8(ciab, CIA_PRB) = r | CIAB_PRBF_DSKSELALL;
    mtx_unlock(&self->mtx);

    return r;
}

// Detects and returns the drive type
uint32_t FloppyController_GetDriveType(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb)
{
    CIAA_BASE_DECL(ciaa);
    CIAB_BASE_DECL(ciab);
    uint32_t dt = 0;

    mtx_lock(&self->mtx);

    // Reset the drive's serial register
    _FloppyController_SetMotor(self, cb, true);
    delay_us(1);
    _FloppyController_SetMotor(self, cb, false);

    // Read the bits from MSB to LSB
    uint8_t r = *cb;
    for (int bit = 31; bit >= 0; bit--) {
        *CIA_REG_8(ciab, CIA_PRB) = r;
        delay_us(1);
        const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
        const uint32_t rdy = (~(r >> CIAA_PRAB_DSKRDY)) & 1u;
        dt |= (rdy << (uint32_t)bit);
        delay_us(1);
        *CIA_REG_8(ciab, CIA_PRB) = r | CIAB_PRBF_DSKSELALL;
    }

    mtx_unlock(&self->mtx);

    return dt;
}

// Returns the current drive status
uint8_t FloppyController_GetStatus(FloppyControllerRef _Nonnull self, DriveState cb)
{
    CIAA_BASE_DECL(ciaa);
    CIAB_BASE_DECL(ciab);

    mtx_lock(&self->mtx);
    *CIA_REG_8(ciab, CIA_PRB) = cb;
    delay_us(1);
    const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
    delay_us(1);
    *CIA_REG_8(ciab, CIA_PRB) = cb | CIAB_PRBF_DSKSELALL;
    mtx_unlock(&self->mtx);

    return ~r & (CIAA_PRAF_DSKRDY | CIAA_PRAF_DSKTK0 | CIAA_PRAF_DSKWPRO | CIAA_PRAF_DSKCHNG);
}

// Turns the motor for drive 'drive' on or off. This function does not wait for
// the motor to reach its final speed.
static void _FloppyController_SetMotor(FloppyControllerRef _Nonnull _Locked self, DriveState* _Nonnull cb, bool onoff)
{
    CIAB_BASE_DECL(ciab);

    // Make sure that none of the drives are selected since a drive latches the
    // motor state when it is selected 
    *CIA_REG_8(ciab, CIA_PRB) = *CIA_REG_8(ciab, CIA_PRB) | CIAB_PRBF_DSKSELALL;
    delay_us(1);


    // Turn the motor on/off
    const uint8_t r = (onoff) ? *cb & ~CIAB_PRBF_DSKMTR : *cb | CIAB_PRBF_DSKMTR;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    *cb = r;


    // Deselect all drives
    delay_us(1);
    *CIA_REG_8(ciab, CIA_PRB) = r | CIAB_PRBF_DSKSELALL;
}

// Turns the motor for drive 'drive' on or off. This function does not wait for
// the motor to reach its final speed.
void FloppyController_SetMotor(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb, bool onoff)
{
    mtx_lock(&self->mtx);
    _FloppyController_SetMotor(self, cb, onoff);
    mtx_unlock(&self->mtx);
}

void FloppyController_SelectHead(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb, int head)
{
    CIAB_BASE_DECL(ciab);

    mtx_lock(&self->mtx);

    // Update the disk side bit
    const uint8_t r = (head == 0) ? *cb | CIAB_PRBF_DSKSIDE : *cb & ~CIAB_PRBF_DSKSIDE;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    *cb = r;


    // Deselect all drives
    delay_us(1);
    *CIA_REG_8(ciab, CIA_PRB) = r | CIAB_PRBF_DSKSELALL;

    mtx_unlock(&self->mtx);
}

// Steps the drive head one cylinder towards the inside (+1) or the outside (-1)
// of the drive.
void FloppyController_StepHead(FloppyControllerRef _Nonnull self, DriveState cb, int delta)
{
    CIAB_BASE_DECL(ciab);

    mtx_lock(&self->mtx);

    // Update the seek direction bit
    uint8_t r = (delta < 0) ? cb | CIAB_PRBF_DSKDIR : cb & ~CIAB_PRBF_DSKDIR;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    

    // Execute the step pulse
    r |= CIAB_PRBF_DSKSTEP;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    delay_us(1);

    r &= ~CIAB_PRBF_DSKSTEP;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    delay_us(1);

    r |= CIAB_PRBF_DSKSTEP;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    delay_us(1);


    // Deselect all drives
    *CIA_REG_8(ciab, CIA_PRB) = cb | CIAB_PRBF_DSKSELALL;

    mtx_unlock(&self->mtx);
}

// Synchronously reads/writes 'nWords' 16bit words from/to the given word buffer.
// Blocks the caller until the DMA is available and all words have been
// transferred from disk. Returns EOK on success and EDISKChANGE if a disk change
// has been detected.
errno_t FloppyController_Dma(FloppyControllerRef _Nonnull self, DriveState cb, uint16_t precompensation, uint16_t* _Nonnull pData, int16_t nWords, bool bWrite)
{
    decl_try_err();
    CIAA_BASE_DECL(ciaa);
    CIAB_BASE_DECL(ciab);
    CHIPSET_BASE_DECL(cs);
    uint8_t status;

    mtx_lock(&self->mtx);

    while (self->flags.inUse && err == EOK) {
        err = cnd_wait(&self->cv, &self->mtx);
    }
    if (err != EOK) {
        mtx_unlock(&self->mtx);
        return EIO;
    }

    self->flags.inUse = 1;


    // Select the drive and turn off the DMA
    *CIA_REG_8(ciab, CIA_PRB) = cb;
    *CHIPSET_REG_16(cs, DSKLEN) = 0x4000;
    delay_ms(1);


    // Check for disk change
    status = ~(*CIA_REG_8(ciaa, CIA_PRA));
    if ((status & CIAA_PRAF_DSKCHNG) == CIAA_PRAF_DSKCHNG) {
        mtx_unlock(&self->mtx);
        return EDISKCHANGE;
    }


    // Check for disk-write-protected if this is a write
    if (bWrite) {
        if ((status & CIAA_PRAF_DSKWPRO) == CIAA_PRAF_DSKWPRO) {
            mtx_unlock(&self->mtx);
            return EROFS;
        }
    }


    // Prepare the DMA
    *CHIPSET_REG_32(cs, DSKPT) = (uint32_t)pData;
    *CHIPSET_REG_16(cs, ADKCON) = 0x7f00;
    if (bWrite) {
        *CHIPSET_REG_16(cs, ADKCON) = 0x9100 | ((precompensation & 0x03) << 13);
    }
    else {
        *CHIPSET_REG_16(cs, ADKCON) = 0x9500;
        *CHIPSET_REG_16(cs, DSKSYNC) = ADF_MFM_SYNC;
    }
    *CHIPSET_REG_16(cs, DMACON) = 0x8210;

    uint16_t dlen = 0x8000 | (nWords & 0x3fff);
    if (bWrite) {
        dlen |= (1 << 14);
    }


    // Turn DMA on
    *CHIPSET_REG_16(cs, DSKLEN) = dlen;
    *CHIPSET_REG_16(cs, DSKLEN) = dlen;

    mtx_unlock(&self->mtx);


    // Wait for the DMA to complete
    struct timespec now, dly, deadline;
    
    clock_gettime(g_mono_clock, &now);
    timespec_from_ms(&dly, 500);
    timespec_add(&now, &dly, &deadline);
    err = sem_acquire(&self->done, &deadline);


    mtx_lock(&self->mtx);

    // Turn DMA off
    *CHIPSET_REG_16(cs, DSKLEN) = 0x4000;   // Floppy DMA off
    *CHIPSET_REG_16(cs, DMACON) = 0x10;     // Floppy DMA off
    *CHIPSET_REG_16(cs, ADKCON) = 0x400;    // Sync detection off


    // Check for disk change
    status = ~(*CIA_REG_8(ciaa, CIA_PRA));
    if ((status & CIAA_PRAF_DSKCHNG) == CIAA_PRAF_DSKCHNG) {
        err = EDISKCHANGE;
    }


    // Deselect all drives
    *CIA_REG_8(ciab, CIA_PRB) = cb | CIAB_PRBF_DSKSELALL;


    // Wait for everything to settle if we just completed a write
    if (bWrite) {
        delay_ms(2);
    }


    self->flags.inUse = 0;
    cnd_broadcast(&self->cv);
    mtx_unlock(&self->mtx);

    return (err == EOK) ? EOK : EIO;
}


class_func_defs(FloppyController, Driver,
override_func_def(deinit, FloppyController, Object)
override_func_def(onStart, FloppyController, Driver)
);
