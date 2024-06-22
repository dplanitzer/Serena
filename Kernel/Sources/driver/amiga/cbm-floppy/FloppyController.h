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
#include <dispatcher/ConditionVariable.h>
#include <dispatcher/Lock.h>
#include <dispatcher/Semaphore.h>
#include <driver/InterruptController.h>

typedef uint8_t   DriveState;       // Per-drive hardware state


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
    Lock                lock;       // Used to ensure that we issue commands to the hardware atomically since all drives share the same CIA and DMA register set
    ConditionVariable   cv;
    Semaphore           done;       // Semaphore indicating whether the DMA is done
    InterruptHandlerID  irqHandler;
    struct __fdcFlags {
        unsigned int        inUse:1;
        unsigned int        reserved:31;
    }                   flags;
} FloppyController;


// Creates the floppy controller
extern errno_t FloppyController_Create(FloppyController* _Nullable * _Nonnull pOutSelf);

extern DriveState FloppyController_Reset(FloppyController* _Nonnull self, int drive);

extern uint32_t FloppyController_GetDriveType(FloppyController* _Nonnull self, DriveState* _Nonnull cb);
extern uint8_t FloppyController_GetStatus(FloppyController* _Nonnull self, DriveState cb);

extern void FloppyController_SetMotor(FloppyController* _Nonnull self, DriveState* _Nonnull cb, bool onoff);
extern void FloppyController_SelectHead(FloppyController* _Nonnull self, DriveState* _Nonnull cb, int head);
extern void FloppyController_StepHead(FloppyController* _Nonnull self, DriveState cb, int delta);

extern errno_t FloppyController_DoIO(FloppyController* _Nonnull self, DriveState cb, uint16_t* _Nonnull pData, int nwords, bool bWrite);

#endif /* FloppyController_h */
