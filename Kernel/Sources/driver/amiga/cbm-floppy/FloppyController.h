//
//  FloppyController.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyController_h
#define FloppyController_h

#include <klib/klib.h>
#include <dispatcher/Semaphore.h>
#include <driver/InterruptController.h>

// CIABPRA bits (FDC status byte)
#define CIABPRA_BIT_DSKRDY      5
#define CIABPRA_BIT_DSKTRACK0   4
#define CIABPRA_BIT_DSKPROT     3
#define CIABPRA_BIT_DSKCHANGE   2
#define CIABPRA_BIT_IODONE      0   /* see fdc_get_io_status() */


// CIABPRB bits (FDC control byte)
#define CIABPRB_BIT_DSKMOTOR    7
#define CIABPRB_BIT_DSKSEL3     6
#define CIABPRB_BIT_DSKSEL2     5
#define CIABPRB_BIT_DSKSEL1     4
#define CIABPRB_BIT_DSKSEL0     3
#define CIABPRB_BIT_DSKSIDE     2
#define CIABPRB_BIT_DSKDIREC    1
#define CIABPRB_BIT_DSKSTEP     0


typedef uint8_t   FdcControlByte;       // shadow copy of the CIABRPB register

extern unsigned int fdc_get_drive_status(FdcControlByte* _Nonnull fdc);
extern void fdc_set_drive_motor(FdcControlByte* _Nonnull fdc, int onoff);
extern void fdc_step_head(FdcControlByte* _Nonnull fdc, int inout);
extern void fdc_select_head(FdcControlByte* _Nonnull fdc, int side);
extern void fdc_io_begin(FdcControlByte* _Nonnull fdc, uint16_t* pData, int nwords, int readwrite);
extern unsigned int fdc_get_io_status(FdcControlByte* _Nonnull fdc);
extern void fdc_io_end(FdcControlByte*  _Nonnull fdc);


// The floppy controller. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
typedef struct FloppyController {
    Semaphore           inuse;  // Semaphore indicating whether the DMA is in use
    Semaphore           done;   // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
} FloppyController;


extern FloppyController* gFloppyController;

// Creates the floppy controller
extern errno_t FloppyController_Create(FloppyController* _Nullable * _Nonnull pOutFloppyController);

extern bool FloppyController_IsReadOnly(FloppyController* _Nonnull self, int drive);

extern errno_t FloppyController_Read(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords);
extern errno_t FloppyController_Write(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, const uint16_t* _Nonnull pData, int nwords);

#endif /* FloppyController_h */
