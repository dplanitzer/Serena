//
//  FloppyController.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyController.h"
#include <hal/Platform.h>

// CIABPRA bits (FDC status byte)
#define CIABPRA_BIT_DSKRDY      5
#define CIABPRA_BIT_DSKTRACK0   4
#define CIABPRA_BIT_DSKPROT     3
#define CIABPRA_BIT_DSKCHANGE   2


// CIABPRB bits (FDC control byte)
#define CIABPRB_BIT_DSKMOTOR    7
#define CIABPRB_BIT_DSKSEL3     6
#define CIABPRB_BIT_DSKSEL2     5
#define CIABPRB_BIT_DSKSEL1     4
#define CIABPRB_BIT_DSKSEL0     3
#define CIABPRB_BIT_DSKSIDE     2
#define CIABPRB_BIT_DSKDIREC    1
#define CIABPRB_BIT_DSKSTEP     0

#define CIABPRB_DSKSELALL ((1 << CIABPRB_BIT_DSKSEL3) | (1 << CIABPRB_BIT_DSKSEL2) | (1 << CIABPRB_BIT_DSKSEL1) | (1 << CIABPRB_BIT_DSKSEL0))


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

FdcControlByte FloppyController_Reset(FloppyController* _Nonnull self, int drive)
{
    CIAB_BASE_DECL(ciab);
    uint8_t r;

    // motor off; all drives deselected; head 0; stepping off
    r = (1 << CIABPRB_BIT_DSKMOTOR) | CIABPRB_DSKSELALL | (1 << CIABPRB_BIT_DSKSTEP);
    
    // select 'drive'
    r &= ~(1 << (CIABPRB_BIT_DSKSEL0 + (drive & 0x03)));

    // Make sure that the motor is off and then deselect the drive
    *CIA_REG_8(ciab, CIA_PRB) = r;
    fdc_nano_delay();
    *CIA_REG_8(ciab, CIA_PRB) = r | CIABPRB_DSKSELALL;

    return r;
}

// Detects and returns the drive type
uint32_t FloppyController_GetDriveType(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb)
{
    CIAA_BASE_DECL(ciaa);
    CIAB_BASE_DECL(ciab);
    uint32_t dt = 0;

    // Reset the drive's serial register
    FloppyController_SetMotor(self, cb, true);
    fdc_nano_delay();
    FloppyController_SetMotor(self, cb, false);

    // Read the bits from MSB to LSB
    uint8_t r = *cb;
    for (int bit = 31; bit >= 0; bit--) {
        *CIA_REG_8(ciab, CIA_PRB) = r;
        const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
        const uint32_t rdy = (~(r >> CIABPRA_BIT_DSKRDY)) & 1u;
        dt |= (rdy << (uint32_t)bit);
        *CIA_REG_8(ciab, CIA_PRB) = r | CIABPRB_DSKSELALL;
    }

    return dt;
}

// Returns the current drive status
uint8_t FloppyController_GetStatus(FloppyController* _Nonnull self, FdcControlByte cb)
{
    CIAA_BASE_DECL(ciaa);
    CIAB_BASE_DECL(ciab);

    *CIA_REG_8(ciab, CIA_PRB) = cb;
    const uint8_t r = *CIA_REG_8(ciaa, CIA_PRA);
    *CIA_REG_8(ciab, CIA_PRB) = cb | CIABPRB_DSKSELALL;

    return ~r & ((1 << CIABPRA_BIT_DSKRDY) | (1 << CIABPRA_BIT_DSKTRACK0) | (1 << CIABPRA_BIT_DSKPROT) | (1 << CIABPRA_BIT_DSKCHANGE));
}

// Turns the motor for drive 'drive' on or off. This function does not wait for
// the motor to reach its final speed.
void FloppyController_SetMotor(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb, bool onoff)
{
    CIAB_BASE_DECL(ciab);

    // Make sure that none of the drives are selected since a drive latches the
    // motor state when it is selected 
    *CIA_REG_8(ciab, CIA_PRB) = *CIA_REG_8(ciab, CIA_PRB) | CIABPRB_DSKSELALL;
    fdc_nano_delay();


    // Turn the motor on/off
    const uint8_t bit = (1 << CIABPRB_BIT_DSKMOTOR);
    const uint8_t r = (onoff) ? *cb & ~bit : *cb | bit;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    *cb = r;


    // Deselect all drives
    *CIA_REG_8(ciab, CIA_PRB) = r | CIABPRB_DSKSELALL;
}

void FloppyController_SetHead(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb, int head)
{
    CIAB_BASE_DECL(ciab);

    // Update the disk side bit
    const uint8_t bit = (1 << CIABPRB_BIT_DSKSIDE);
    const uint8_t r = (head == 0) ? *cb | bit : *cb & ~bit;
    *CIA_REG_8(ciab, CIA_PRB) = r;
    *cb = r;


    // Deselect all drives
    *CIA_REG_8(ciab, CIA_PRB) = r | CIABPRB_DSKSELALL;
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
    fdc_io_end(pFdc);
    
    Semaphore_Relinquish(&self->inuse);
    //print("DMA done (%d)\n", (int)err);
    //while(1);

    return (err == ETIMEDOUT) ? ENOMEDIUM : err;

catch:
    return err;
}
