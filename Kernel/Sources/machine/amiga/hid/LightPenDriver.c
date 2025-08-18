//
//  LightPenDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "LightPenDriver.h"
#include <machine/amiga/chipset.h>


final_class_ivars(LightPenDriver, InputDriver,
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

IOCATS_DEF(g_cats, IOHID_LIGHTPEN);


errno_t LightPenDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    LightPenDriverRef self;

    if (port < 0 || port > 1) {
        throw(ENODEV);
    }
    
    try(Driver_Create(class(LightPenDriver), g_cats, kDriver_Exclusive, parentDirId, (DriverRef*)&self));
    
    CHIPSET_BASE_DECL(cp);

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
    
catch:
    *pOutSelf = (DriverRef)self;
    return err;
}

errno_t LightPenDriver_onStart(LightPenDriverRef _Nonnull _Locked self)
{
    decl_try_err();
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
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    err = Driver_Publish(self, &de);
    if (err == EOK) {
        CHIPSET_BASE_DECL(cp);

        // Switch POTGO bits 8 to 11 to output / high data for the middle and right mouse buttons
        *CHIPSET_REG_16(cp, POTGO) = *CHIPSET_REG_16(cp, POTGO) & 0x0f00;
    }
    return err;
}

void LightPenDriver_onStop(DriverRef _Nonnull _Locked self)
{
    Driver_Unpublish(self);
}

// Returns the current position of the light pen if the light pen triggered.
static bool _get_lp_position(int16_t* _Nonnull x, int16_t* _Nonnull y)
{
    CHIPSET_BASE_DECL(cp);
    bool r = false;

    // Read VHPOSR first time
    const uint32_t posr0 = *CHIPSET_REG_32(cp, VPOSR);


    // Wait for scanline microseconds
    const uint32_t hsync0 = chipset_get_hsync_counter();
    const uint16_t bplcon0 = *CHIPSET_REG_16(cp, BPLCON0);
    while (chipset_get_hsync_counter() == hsync0);
    

    // Read VHPOSR a second time
    const uint32_t posr1 = *CHIPSET_REG_32(cp, VPOSR);
    

    
    // Check whether the light pen triggered
    // See Amiga Reference Hardware Manual p233.
    if (posr0 == posr1) {
        if ((posr0 & 0x0000ffff) < 0x10500) {
            *x = (posr0 & 0x000000ff) << 1;
            *y = (posr0 & 0x1ff00) >> 8;
            
            if ((bplcon0 & BPLCON0F_LACE) != 0 && ((posr0 & 0x8000) != 0)) {
                // long frame (odd field) is offset in Y by one
                *y += 1;
            }
            r = true;
        }
    }

    return r;
}

void LightPenDriver_getReport(LightPenDriverRef _Nonnull self, HIDReport* _Nonnull report)
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

        if (_get_lp_position(&xPos, &yPos)) {
            self->triggerCount++;
            self->sumX += xPos;
            self->sumY += yPos;
        }
    }

    
    // Button #0
    register uint16_t potgor = *(self->reg_potgor);
    if ((potgor & self->right_button_mask) == 0) {
        buttons |= 0x02;
    }
    
    
    // Button # 1
    if ((potgor & self->middle_button_mask) == 0) {
        buttons |= 0x04;
    }
    

    report->type = kHIDReportType_LightPen;
    report->data.lp.x = x;
    report->data.lp.y = y;
    report->data.lp.buttons = buttons;
    report->data.lp.hasPosition = hasPosition;
}


class_func_defs(LightPenDriver, InputDriver,
override_func_def(onStart, LightPenDriver, Driver)
override_func_def(onStop, LightPenDriver, Driver)
override_func_def(getReport, LightPenDriver, InputDriver)
);
