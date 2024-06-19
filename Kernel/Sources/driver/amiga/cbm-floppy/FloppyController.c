//
//  FloppyController.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyController.h"
#include <hal/Platform.h>

static void FloppyController_Destroy(FloppyController* _Nullable self);


// Creates the floppy controller
errno_t FloppyController_Create(FloppyController* _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    FloppyController* self;
    
    try(kalloc_cleared(sizeof(FloppyController), (void**) &self));

    Semaphore_Init(&self->inuse, 1);
    Semaphore_Init(&self->done, 0);
        
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
    FloppyController_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

// Destroys the floppy controller.
static void FloppyController_Destroy(FloppyController* _Nullable self)
{
    if (self) {
        if (self->irqHandler != 0) {
            try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
        }
        self->irqHandler = 0;
        
        Semaphore_Deinit(&self->inuse);
        Semaphore_Deinit(&self->done);
        
        kfree(self);
    }
}

// Selects or deselects the given drive
static void fdc_select_drive(int drive, bool doSelect)
{
    CIAB_BASE_DECL(ciab);
    uint8_t sel = 0;

    if (drive >= 0 && drive < 4) {
        sel = 1 << (CIABPRB_BIT_DSKSEL0 + drive);
    }
    else {
        abort();
    }

    if (doSelect) {
        *CIA_REG_8(ciab, CIA_PRB) &= ~sel;
    }
    else {
        *CIA_REG_8(ciab, CIA_PRB) |= sel;
    }
}

// Detects and returns the drive type
uint32_t FloppyController_GetDriveType(FloppyController* _Nonnull self, int drive)
{
    CIAA_BASE_DECL(ciaa);
    uint32_t dt = 0;

    // Reset the drive's serial register
    FloppyController_SetMotor(self, drive, true);
    FloppyController_SetMotor(self, drive, false);

    // Read the bits from MSB to LSB
    for (int bit = 31; bit >= 0; bit--) {
        fdc_select_drive(drive, true);
        const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
        const uint32_t rdy = (~(r >> CIABPRA_BIT_DSKRDY) & 1);
        dt |= rdy << bit;
        fdc_select_drive(drive, false);
    }

    return dt;
}

// Returns true if the drive 'drive' is hardware write protected; false otherwise
bool FloppyController_IsReadOnly(FloppyController* _Nonnull self, int drive)
{
    fdc_select_drive(drive, true);
    
    CIAA_BASE_DECL(ciaa);
    const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
    
    fdc_select_drive(drive, false);

    return ((r & (1 << CIABPRA_BIT_DSKPROT)) == 0) ? true : false;
}

// Turns the motor for drive 'drive' on or off. This function does not wait for
// the motor to reach its final speed.
void FloppyController_SetMotor(FloppyController* _Nonnull self, int drive, bool onoff)
{
    CIAB_BASE_DECL(ciab);

    fdc_select_drive(drive, false);
    fdc_nano_delay();

    // The drive latches the new motor state when we select it
    if (onoff) {
        *CIA_REG_8(ciab, CIA_PRB) &= ~CIABPRB_BIT_DSKMOTOR;
    }
    else {
        *CIA_REG_8(ciab, CIA_PRB) |= CIABPRB_BIT_DSKMOTOR;
    }
    fdc_select_drive(drive, true);
    fdc_nano_delay();

    fdc_select_drive(drive, false);
}

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
errno_t FloppyController_DoIO(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords, bool readwrite)
{
    decl_try_err();
    
    try(Semaphore_Acquire(&self->inuse, kTimeInterval_Infinity));
    //print("b, buffer: %p, nwords: %d\n", pData, nwords);
    fdc_io_begin(pFdc, pData, nwords, (int)readwrite);
    err = Semaphore_Acquire(&self->done, kTimeInterval_Infinity);
    if (err == EOK) {
        const unsigned int status = fdc_get_io_status(pFdc);
        
        if ((status & (1 << CIABPRA_BIT_DSKRDY)) != 0) {
            err = ENOMEDIUM;
        }
        else if ((status & (1 << CIABPRA_BIT_DSKCHANGE)) == 0) {
            err = EDISKCHANGE;
        }
    }
    fdc_io_end(pFdc);
    
    Semaphore_Relinquish(&self->inuse);
    //print("DMA done (%d)\n", (int)err);
    //while(1);

    return (err == ETIMEDOUT) ? ENOMEDIUM : err;

catch:
    return err;
}
