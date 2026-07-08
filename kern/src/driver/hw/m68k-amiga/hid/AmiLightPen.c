//
//  AmiLightPen.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AmiLightPen.h"
#include <driver/IORegistry.h>
#include <driver/hid/IOHIDBeamDevice.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <hal/hw/m68k-amiga/cia8520.h>


final_class_ivars(AmiLightPen, IOHIDDevice,
    IODriverRef beamDevice;
    uint16_t    right_button_mask;
    uint16_t    middle_button_mask;
    int16_t     smoothedX;
    int16_t     smoothedY;
    bool        hasSmoothedPosition;    // True if the light pen position is available (pen triggered the position latching hardware); false otherwise
    int16_t     sumX;
    int16_t     sumY;
    int8_t      sampleCount;    // How many samples to average to produce a smoothed value
    int8_t      sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t      triggerCount;   // Number of times that the light pen has triggered in the 'sampleCount' interval
    int8_t      port;
);

IOCATS_DEF(g_cats, IOHID_LIGHTPEN);
IOCATS_DEF(g_hid_beam_cats, IOHID_BEAM);


errno_t AmiLightPen_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiLightPenRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(IODriver_Create(class(AmiLightPen), g_cats, (IODriverRef*)&self));
    self->port = (int8_t)port;
    
catch:
    *pOutSelf = (IODriverRef)self;
    return err;
}

errno_t AmiLightPen_start(AmiLightPenRef _Nonnull self)
{
    decl_try_err();

    //XXX guess we should look for all beam devices? Probably with live matching?
    self->beamDevice = IORegistry_CopyBestMatchingDriver(gIORegistry, g_hid_beam_cats);
    if (self->beamDevice) {
        err = IODriver_Open(self->beamDevice, O_RDONLY);
    }
    else {
        err = ENODEV;
    }

    if (err != EOK) {
        return err;
    }


    // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
    hw_chips->potgo &= 0x0f00;


    if (self->port == 0) {
        self->right_button_mask = POTGORF_DATLY;
        self->middle_button_mask = POTGORF_DATLX;
    }
    else {
        self->right_button_mask = POTGORF_DATRY;
        self->middle_button_mask = POTGORF_DATRX;
    }

    self->smoothedX = 0;
    self->smoothedY = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->hasSmoothedPosition = false;
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->triggerCount = 0;

    return EOK;
}

bool AmiLightPen_stop(AmiLightPenRef _Nonnull self)
{
    if (self->beamDevice) {
        IODriver_Close(self->beamDevice);
        Object_Release(self->beamDevice);
        self->beamDevice = NULL;
    }

    return true;
}

void AmiLightPen_getReport(AmiLightPenRef _Nonnull self, IOHIDReport* _Nonnull report)
{
    // Return the smoothed value
    int16_t x = self->smoothedX;
    int16_t y = self->smoothedY;
    const bool hasPosition = self->hasSmoothedPosition;
    uint32_t buttons = 0;

    
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

        if (IOHIDBeamDevice_GetBeamPosition(self->beamDevice, &xPos, &yPos)) {
            self->triggerCount++;
            self->sumX += xPos;
            self->sumY += yPos;
        }
    }

    
    // Button #0
    const uint16_t potinp = hw_chips->potinp;
    if ((potinp & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }
    
    
    // Button # 1
    if ((potinp & self->middle_button_mask) == 0) {
        buttons |= 0x04;
    }
    

    report->type = kIOHIDReportType_LightPen;
    report->data.lp.x = x;
    report->data.lp.y = y;
    report->data.lp.buttons = buttons;
    report->data.lp.hasPosition = hasPosition;
}


class_func_defs(AmiLightPen, IOHIDDevice,
override_func_def(start, AmiLightPen, IODriver)
override_func_def(getReport, AmiLightPen, IOHIDDevice)
);
