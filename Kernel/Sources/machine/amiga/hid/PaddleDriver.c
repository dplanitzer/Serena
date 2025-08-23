//
//  PaddleDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "PaddleDriver.h"
#include <machine/amiga/chipset.h>


final_class_ivars(PaddleDriver, InputDriver,
    volatile uint16_t* _Nonnull reg_joydat;
    volatile uint16_t* _Nonnull reg_potdat;
    volatile uint16_t* _Nonnull reg_potgo;
    int16_t                     smoothedX;
    int16_t                     smoothedY;
    int16_t                     sumX;
    int16_t                     sumY;
    int8_t                      sampleCount;    // How many samples to average to produce a smoothed value
    int8_t                      sampleIndex;    // Current sample in the range 0..<sampleCount
    int8_t                      port;
);

IOCATS_DEF(g_cats, IOHID_ANALOG_JOYSTICK);


errno_t PaddleDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    PaddleDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(PaddleDriver), kDriver_Exclusive, g_cats, (DriverRef*)&self));

    CHIPSET_BASE_DECL(cp);

    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potdat = (port == 0) ? CHIPSET_REG_16(cp, POT0DAT) : CHIPSET_REG_16(cp, POT1DAT);
    self->reg_potgo = CHIPSET_REG_16(cp, POTGO);
    self->port = (int8_t)port;
    self->sampleCount = 4;
    self->sampleIndex = 0;
    self->sumX = 0;
    self->sumY = 0;
    self->smoothedX = 0;
    self->smoothedY = 0;

catch:
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t PaddleDriver_onStart(PaddleDriverRef _Nonnull _Locked self)
{
    char name[8];

    name[0] = 'p';
    name[1] = 'a';
    name[2] = 'd';
    name[3] = 'd';
    name[4] = 'l';
    name[5] = 'e';
    name[6] = '0' + self->port;
    name[7] = '\0';

    DriverEntry de;
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.arg = 0;

    return Driver_Publish((DriverRef)self, &de);
}

void PaddleDriver_getReport(PaddleDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    register uint16_t potdat = *(self->reg_potdat);
    register uint16_t joydat = *(self->reg_joydat);

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
    *(self->reg_potgo) = 0x0001;
    
    
    report->type = kHIDReportType_Joystick;
    report->data.joy.x = x;
    report->data.joy.y = y;
    report->data.joy.buttons = buttons;
}


class_func_defs(PaddleDriver, InputDriver,
override_func_def(onStart, PaddleDriver, Driver)
override_func_def(getReport, PaddleDriver, InputDriver)
);
