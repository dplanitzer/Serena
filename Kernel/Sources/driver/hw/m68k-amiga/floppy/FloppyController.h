//
//  FloppyController.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyController_h
#define FloppyController_h

#include <driver/Driver.h>


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


enum Precompensation {
    kPrecompensation_0ns = 0,
    kPrecompensation_140ns = 1,
    kPrecompensation_280ns = 2,
    kPrecompensation_560ns = 3
};

typedef struct DriveParams {
    int8_t  driveType;
    int8_t  heads;
    int8_t  cylinders;
    int8_t  precomp_00;     // 0ns
    int8_t  precomp_01;     // 140ns
    int8_t  precomp_10;     // 280ns
    int8_t  precomp_11;     // 560ns
    int8_t  retryCount;
} DriveParams;


// The floppy controller. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
final_class(FloppyController, Driver);


extern errno_t FloppyController_Create(FloppyControllerRef _Nullable * _Nonnull pOutSelf);


//
// Internal APIs
//

extern const DriveParams   kDriveParams_3_5;
extern const DriveParams   kDriveParams_5_25;

extern DriveState FloppyController_ResetDrive(FloppyControllerRef _Nonnull self, int drive);

extern uint32_t FloppyController_GetDriveType(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb);
extern uint8_t FloppyController_GetStatus(FloppyControllerRef _Nonnull self, DriveState cb);

extern void FloppyController_SetMotor(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb, bool onoff);
extern void FloppyController_SelectHead(FloppyControllerRef _Nonnull self, DriveState* _Nonnull cb, int head);
extern void FloppyController_StepHead(FloppyControllerRef _Nonnull self, DriveState cb, int delta);

extern errno_t FloppyController_Dma(FloppyControllerRef _Nonnull self, DriveState cb, uint16_t precompensation, uint16_t* _Nonnull pData, int16_t nWords, bool bWrite);

#endif /* FloppyController_h */
