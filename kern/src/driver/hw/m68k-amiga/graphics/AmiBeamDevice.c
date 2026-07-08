//
//  AmiBeamDevice.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "AmiBeamDevice.h"
#include "gd.h"
#include <hal/hw/m68k-amiga/chipset.h>
#include <hal/hw/m68k-amiga/cia8520.h>
#include <kpi/hid.h>

IOCATS_DEF(g_cats, IOHID_BEAM);


errno_t AmiBeamDevice_Create(AmiBeamDeviceRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiBeamDeviceRef self;
    
    try(IODriver_Create(class(AmiBeamDevice), g_cats, (IODriverRef*)&self));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AmiBeamDevice_doOpen(IODriverRef _Nonnull _Locked self, fd_flags_t flags)
{
    const errno_t err = IODriver_Super_DoOpen(AmiBeamDevice, flags);

    if (err == EOK) {
        gdLock();
        gdSetLightPenEnabled(true);
        gdUnlock();
    }

    return err;
}

void AmiBeamDevice_doClose(IODriverRef _Nonnull _Locked self)
{
    IODriver_Super_DoClose(AmiBeamDevice);

    gdLock();
    gdSetLightPenEnabled(false);
    gdUnlock();
}

static void _wait_for_next_scanline(void)
{
    const uint8_t oh = hw_cia_b->todhi;
    const uint8_t om = hw_cia_b->todmid;
    const uint8_t ol = hw_cia_b->todlo;

    for (;;) {
        if (hw_cia_b->todlo != ol || hw_cia_b->todmid != om || hw_cia_b->todhi != oh) {
            break;
        }
    }
}

bool AmiBeamDevice_getBeamPosition(AmiBeamDeviceRef _Nonnull self, int16_t* _Nonnull x, int16_t* _Nonnull y)
{
    bool r = false;

    // Read VHPOSR first time
    const uint32_t posr0 = hw_chips->vposr;


    // Wait for "scanline" microseconds
    _wait_for_next_scanline();
    

    // Read VHPOSR a second time
    const uint32_t posr1 = hw_chips->vposr;
    

    
    // Check whether the light pen triggered
    // See Amiga Reference Hardware Manual p233.
    if (posr0 == posr1) {
        if ((posr0 & 0x0000ffff) < 0x10500) {
            *x = (posr0 & 0x000000ff) << 1;
            *y = (posr0 & 0x1ff00) >> 8;
            
            if ((hw_chips->bplcon0 & BPLCON0F_LACE) != 0 && ((posr0 & 0x8000) != 0)) {
                // long frame (odd field) is offset in Y by one
                *y += 1;
            }
            r = true;
        }
    }

    return r;
}


class_func_defs(AmiBeamDevice, IOHIDBeamDevice,
override_func_def(doOpen, AmiBeamDevice, IODriver)
override_func_def(doClose, AmiBeamDevice, IODriver)
override_func_def(getBeamPosition, AmiBeamDevice, IOHIDBeamDevice)
);
