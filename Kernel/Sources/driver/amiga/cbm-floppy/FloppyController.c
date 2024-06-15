//
//  FloppyController.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "FloppyController.h"

static void FloppyController_Destroy(FloppyController* _Nullable self);

FloppyController* gFloppyController;


// Creates the floppy controller
errno_t FloppyController_Create(FloppyController* _Nullable * _Nonnull pOutFloppyController)
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
    *pOutFloppyController = self;
    return EOK;

catch:
    FloppyController_Destroy(self);
    *pOutFloppyController = NULL;
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

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
static errno_t FloppyController_DoIO(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords, int readwrite)
{
    decl_try_err();
    
    try(Semaphore_Acquire(&self->inuse, kTimeInterval_Infinity));
    //print("b, buffer: %p, nwords: %d\n", pData, nwords);
    fdc_io_begin(pFdc, pData, nwords, 0);
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

// Synchronously reads 'nwords' 16bit words into the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred from
// disk.
errno_t FloppyController_Read(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords)
{
//    print("DMA_Read ");
    return FloppyController_DoIO(self, pFdc, pData, nwords, 0);
}

// Synchronously writes 'nwords' 16bit words from the given word buffer. Blocks
// the caller until the DMA is available and all words have been transferred to
// disk.
errno_t FloppyController_Write(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, const uint16_t* _Nonnull pData, int nwords)
{
//    print("DMA_Write ");
    return FloppyController_DoIO(self, pFdc, (uint16_t*)pData, nwords, 1);
}
