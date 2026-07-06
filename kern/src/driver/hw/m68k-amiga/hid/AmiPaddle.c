//
//  AmiPaddle.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AmiPaddle.h"
#include <hal/hw/m68k-amiga/chipset.h>


final_class_ivars(AmiPaddle, IOHIDDevice,
    int16_t smoothedX;
    int16_t smoothedY;
    int16_t sumX;
    int16_t sumY;
    int8_t  sampleCount;    // How many samples to average to produce a smoothed value
    int8_t  sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t  port;
);

IOCATS_DEF(g_cats, IOHID_ANALOG_JOYSTICK);


errno_t AmiPaddle_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiPaddleRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(IODriver_Create(class(AmiPaddle), g_cats, (IODriverRef*)&self));
    self->port = (int8_t)port;

catch:
    *pOutSelf = (IODriverRef)self;
    return err;
}

errno_t AmiPaddle_start(AmiPaddleRef _Nonnull self)
{
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->smoothedX = 0;
    self->smoothedY = 0;

    return EOK;
}

void AmiPaddle_getReport(AmiPaddleRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    register const uint16_t potdat = (self->port == 0) ? hw_chips->joy0dat : hw_chips->joy1dat;
    register const uint16_t joydat = (self->port == 0) ? hw_chips->pot0dat : hw_chips->pot1dat;

    // Return the smoothed value
    const int16_t x = self->smoothedX;
    const int16_t y = self->smoothedY;
    uint8_t buttons = 0;
    
    
    // Sum up to 'sampleCount' samples and then compute the smoothed out value
    // as the average of 'sampleCount' samples.
    if (self->sampleIndex == self->sampleCount) {
        self->smoothedX = (self->sumX / self->sampleCount) << 8;
        self->smoothedY = (self->sumY / self->sampleCount) << 8;
        self->sampleIndex = 0;
        self->sumX = 0;
        self->sumY = 0;
    } else {
        self->sampleIndex++;
        
        // X axis
        const int16_t xval = (int16_t)(potdat & 0x00ff);
        self->sumX += (xval - 128);
        
        
        // Y axis
        const int16_t yval = (int16_t)((potdat >> 8) & 0x00ff);
        self->sumY += (yval - 128);
    }

    
    // Left fire button
    if ((joydat & (1 << 9)) != 0) {
        buttons |= 0x01;
    }
    
    
    // Right fire button
    if ((joydat & (1 << 1)) != 0) {
        buttons |= 0x02;
    }
    
    
    // Restart the counter for the next frame
    hw_chips->potgo = 0x0001;
    
    
    report->type = kIOHIDReportType_GamePad;
    report->data.joy.x = x;
    report->data.joy.y = y;
    report->data.joy.buttons = buttons;
}


class_func_defs(AmiPaddle, IOHIDDevice,
override_func_def(start, AmiPaddle, IODriver)
override_func_def(getReport, AmiPaddle, IOHIDDevice)
);
