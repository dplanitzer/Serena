//
//  LightPenDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "LightPenDriver.h"
#include <hal/InterruptController.h>


final_class_ivars(LightPenDriver, Driver,
    EventDriverRef _Nonnull     eventDriver;
    GraphicsDriverRef _Nonnull  gdevice;
    InterruptHandlerID          irqHandler;
    volatile uint16_t*            reg_potgor;
    uint16_t                      right_button_mask;
    uint16_t                      middle_button_mask;
    int16_t                       smoothedX;
    int16_t                       smoothedY;
    bool                        hasSmoothedPosition;    // True if the light pen position is available (pen triggered the position latching hardware); false otherwise
    int16_t                       sumX;
    int16_t                       sumY;
    int8_t                        sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                        sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                        triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    int8_t                        port;
);

extern void LightPenDriver_OnInterrupt(LightPenDriverRef _Nonnull self);


errno_t LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, LightPenDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    CHIPSET_BASE_DECL(cp);
    LightPenDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(LightPenDriver, 0, &self));
    
    self->eventDriver = Object_RetainAs(pEventDriver, EventDriver);
    self->gdevice = Object_RetainAs(EventDriver_GetGraphicsDriver(pEventDriver), GraphicsDriver);
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

    *pOutSelf = self;
    return EOK;
    
catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static void LightPenDriver_deinit(LightPenDriverRef _Nonnull self)
{
    try_bang(InterruptController_RemoveInterruptHandler(gInterruptController, self->irqHandler));
    
    Object_Release(self->gdevice);
    self->gdevice = NULL;

    Object_Release(self->eventDriver);
    self->eventDriver = NULL;
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
        
        if (GraphicsDriver_GetLightPenPosition(self->gdevice, &xPos, &yPos)) {
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
    

    EventDriver_ReportLightPenDeviceChange(self->eventDriver, xAbs, yAbs, hasPosition, buttonsDown);
}


class_func_defs(LightPenDriver, Driver,
override_func_def(deinit, LightPenDriver, Object)
);
