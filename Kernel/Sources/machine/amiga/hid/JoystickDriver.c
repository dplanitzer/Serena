//
//  JoystickDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "JoystickDriver.h"
#include <machine/amiga/chipset.h>



////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Digital Joystick Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(DigitalJoystickDriver, InputDriver,
    volatile uint16_t* _Nonnull reg_joydat;
    volatile uint16_t* _Nonnull reg_potgor;
    volatile uint8_t* _Nonnull  reg_ciaa_pra;
    uint16_t                    right_button_mask;
    uint8_t                     fire_button_mask;
    int8_t                      port;
);

IOCATS_DEF(g_cats_digjoy, IOHID_DIGITAL_JOYSTICK);


errno_t DigitalJoystickDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DigitalJoystickDriverRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(DigitalJoystickDriver), g_cats_digjoy, kDriver_Exclusive, parentDirId, (DriverRef*)&self));
    
    CHIPSET_BASE_DECL(cp);
    CIAA_BASE_DECL(ciaa);

    self->reg_joydat = (port == 0) ? CHIPSET_REG_16(cp, JOY0DAT) : CHIPSET_REG_16(cp, JOY1DAT);
    self->reg_potgor = CHIPSET_REG_16(cp, POTGOR);
    self->reg_ciaa_pra = CIA_REG_8(ciaa, 0);
    self->right_button_mask = (port == 0) ? POTGORF_DATLY : POTGORF_DATRY;
    self->fire_button_mask = (port == 0) ? CIAA_PRAF_FIR0 : CIAA_PRAF_FIR1;
    self->port = (int8_t)port;
    
catch:
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t DigitalJoystickDriver_onStart(DigitalJoystickDriverRef _Nonnull _Locked self)
{
    decl_try_err();
    char name[6];

    name[0] = 'd';
    name[1] = 'j';
    name[2] = 'o';
    name[3] = 'y';
    name[4] = '0' + self->port;
    name[5] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    err = Driver_Publish(self, &de);
    if (err == EOK) {
        CHIPSET_BASE_DECL(cp);
        CIAA_BASE_DECL(ciaa);

        // Switch bit 7 and 6 to input
        *CIA_REG_8(ciaa, CIA_DDRA) = *CIA_REG_8(ciaa, CIA_DDRA) & 0x3f;
    
        // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
        *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    }
    return err;
}

void DigitalJoystickDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

void DigitalJoystickDriver_getReport(DigitalJoystickDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    register uint8_t pra = *(self->reg_ciaa_pra);
    register uint16_t joydat = *(self->reg_joydat);
    int16_t x, y;
    uint32_t buttons = 0;

    
    // Left fire button
    if ((pra & self->fire_button_mask) == 0) {
        buttons |= 0x01;
    }
    
    
    // Right fire button
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }

    
    // X axis
    if ((joydat & (1 << 1)) != 0) {
        x = INT16_MAX;  // right
    } else if ((joydat & (1 << 9)) != 0) {
        x = INT16_MIN;  // left
    }
    
    
    // Y axis
    register uint16_t joydat_xor = joydat ^ (joydat >> 1);
    if ((joydat & (1 << 0)) != 0) {
        y = INT16_MAX;  // down
    } else if ((joydat & (1 << 8)) != 0) {
        y = INT16_MIN;  // up
    }


    report->type = kHIDReportType_Joystick;
    report->data.joy.x = x;
    report->data.joy.y = y;
    report->data.joy.buttons = buttons;
}


class_func_defs(DigitalJoystickDriver, InputDriver,
override_func_def(onStart, DigitalJoystickDriver, Driver)
override_func_def(onStop, DigitalJoystickDriver, Driver)
override_func_def(getReport, DigitalJoystickDriver, InputDriver)
);


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Analog Joystick (Paddles) Driver
////////////////////////////////////////////////////////////////////////////////

final_class_ivars(AnalogJoystickDriver, InputDriver,
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

IOCATS_DEF(g_cats_anajoy, IOHID_ANALOG_JOYSTICK);

extern void AnalogJoystickDriver_OnInterrupt(AnalogJoystickDriverRef _Nonnull self);


errno_t AnalogJoystickDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AnalogJoystickDriverRef self = NULL;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(AnalogJoystickDriver), g_cats_anajoy, kDriver_Exclusive, parentDirId, (DriverRef*)&self));

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

errno_t AnalogJoystickDriver_onStart(AnalogJoystickDriverRef _Nonnull _Locked self)
{
    char name[6];

    name[0] = 'a';
    name[1] = 'j';
    name[2] = 'o';
    name[3] = 'y';
    name[4] = '0' + self->port;
    name[5] = '\0';

    DriverEntry de;
    de.dirId = Driver_GetParentDirectoryId(self);
    de.name = name;
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0444);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    return Driver_Publish(self, &de);
}

void AnalogJoystickDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

void AnalogJoystickDriver_getReport(AnalogJoystickDriverRef _Nonnull self, HIDReport* _Nonnull report)
{
    register uint16_t potdat = *(self->reg_potdat);
    register uint16_t joydat = *(self->reg_joydat);

    // Return the smoothed value
    const int16_t x = self->smoothedX;
    const int16_t y = self->smoothedY;
    uint32_t buttons = 0;
    
    
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


class_func_defs(AnalogJoystickDriver, InputDriver,
override_func_def(onStart, AnalogJoystickDriver, Driver)
override_func_def(onStop, AnalogJoystickDriver, Driver)
override_func_def(getReport, AnalogJoystickDriver, InputDriver)
);
