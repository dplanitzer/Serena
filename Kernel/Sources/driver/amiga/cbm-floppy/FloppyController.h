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

typedef uint8_t   FdcControlByte;       // Per-drive hardware state

extern void fdc_io_begin(FdcControlByte* _Nonnull fdc, uint16_t* pData, int nwords, int readwrite);
extern void fdc_io_end(FdcControlByte*  _Nonnull fdc);
extern void fdc_nano_delay(void);


enum DriveType {
    kDriveType_None = 0x00000000,       // No drive connected
    kDriveType_3_5 =  0xffffffff,       // 3.5" drive
    kDriveType_5_25 = 0x55555555,       // 5.25" drive
};


enum DriveStatus {
    kDriveStatus_DiskChanged = 0x04,
    kDriveStatus_IsReadOnly = 0x08,
    kDriveStatus_AtTrack0 = 0x10,
    kDriveStatus_DiskReady = 0x20
};


// The floppy controller. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
typedef struct FloppyController {
    Semaphore           inuse;  // Semaphore indicating whether the DMA is in use
    Semaphore           done;   // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
} FloppyController;


// Creates the floppy controller
extern errno_t FloppyController_Create(FloppyController* _Nullable * _Nonnull pOutSelf);

extern FdcControlByte FloppyController_Reset(FloppyController* _Nonnull self, int drive);

extern uint32_t FloppyController_GetDriveType(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb);

extern uint8_t FloppyController_GetStatus(FloppyController* _Nonnull self, FdcControlByte cb);
extern void FloppyController_SetMotor(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb, bool onoff);
extern void FloppyController_SelectHead(FloppyController* _Nonnull self, FdcControlByte* _Nonnull cb, int head);
extern void FloppyController_StepHead(FloppyController* _Nonnull self, FdcControlByte cb, int delta);

extern errno_t FloppyController_DoIO(FloppyController* _Nonnull self, FdcControlByte* _Nonnull pFdc, uint16_t* _Nonnull pData, int nwords, bool readwrite);

#endif /* FloppyController_h */
