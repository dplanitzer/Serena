//
//  LightPenDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "LightPenDriver.h"
#include <driver/hid/HIDManager.h>
#include <machine/InterruptController.h>
#include <machine/amiga/chipset.h>


final_class_ivars(LightPenDriver, InputDriver,
    InterruptHandlerID          irqHandler;
    volatile uint16_t* _Nonnull reg_potgor;
    uint16_t                    right_button_mask;
    uint16_t                    middle_button_mask;
    int16_t                     smoothedX;
    int16_t                     smoothedY;
    bool                        hasSmoothedPosition;    // True if the light pen position is available (pen triggered the position latching hardware); false otherwise
    int16_t                     sumX;
    int16_t                     sumY;
    int8_t                      sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                      sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                      triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    int8_t                      port;
);

extern void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull self);


errno_t LightPenDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    LightPenDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(LightPenDriver), kDriver_Exclusive, parentDirId, (DriverRef*)&self));
    
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->middle_button_mask = (port == 0) ? POTGORF_DATLX : POTGORF_DATRX;
    self->smoothedX = 0;
    self->smoothedY = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->hasSmoothedPosition = false;
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->triggerCount = 0;
    self->port = (int8_t)port;
    
    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    
    try(InterruptController_AddDirectInterruptHandler(gInterruptController,
                                                      INTERRUPT_ID_VERTICAL_BLANK,
                                                      INTERRUPT_HANDLER_PRIORITY_NORMAL - 1,
                                                      (InterruptHandler_Closure)LightPenDriver_OnInterrupt,
                                                      self,
                                                      &self->irqHandler));
    InterruptController_SetInterruptHandlerEnabled(gInterruptController, self->irqHandler, true);

    *pOutSelf = (DriverRef)self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void LightPenDriver_deinit(LightPenDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
}

errno_t LightPenDriver_onStart(LightPenDriverRef _Nonnull _Locked self)
{
    char name[6];

    name[0] = 'l';
    name[1] = 'p';
    name[2] = 'e';
    name[3] = 'n';
    name[4] = '0' + self->port;
    name[5] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.handler = NULL;
    de.driver = (DriverRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

void LightPenDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

InputType LightPenDriver_getInputType(LightPenDriverRef _Nonnull self)
{
    return kInputType_LightPen;
}

void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull self)
{
    // Return the smoothed value
    int16_t xAbs = self->smoothedX;
    int16_t yAbs = self->smoothedY;
    const bool hasPosition = self->hasSmoothedPosition;
    uint32_t buttonsDown = 0;

    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (self->sampleIndex == self->sampleCount) {
        self->smoothedX = self->triggerCount ? (self->sumX / self->triggerCount) << 8 : 0;
        self->smoothedY = self->triggerCount ? (self->sumY / self->triggerCount) << 8 : 0;
        self->hasSmoothedPosition = self->triggerCount >= (self->sampleCount / 2);
        self->sampleIndex = 0;
        self->triggerCount = 0;
        self->sumX = 0;
        self->sumY = 0;
    } else {
        self->sampleIndex++;
    
        // Get the position
        int16_t xPos, yPos;
        
        if (HIDManager_GetLightPenPosition(gHIDManager, &xPos, &yPos)) {
            self->triggerCount++;
            self->sumX += xPos;
            self->sumY += yPos;
        }
    }

    
    // Button #0
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttonsDown |= 0x02;
    }
    
    
    // Button # 1
    if ((potgor & self->middle_button_mask) == 0) {
        buttonsDown |= 0x04;
    }
    

    HIDManager_ReportLightPenDeviceChange(gHIDManager, xAbs, yAbs, hasPosition, buttonsDown);
}


class_func_defs(LightPenDriver, InputDriver,
override_func_def(deinit, LightPenDriver, Object)
override_func_def(onStart, LightPenDriver, Driver)
override_func_def(onStop, LightPenDriver, Driver)
override_func_def(getInputType, LightPenDriver, InputDriver)
);
