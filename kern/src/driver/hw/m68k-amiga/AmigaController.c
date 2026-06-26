//
//  AmigaController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "AmigaController.h"
#include <ext/endian.h>
#include <ext/math.h>
#include <driver/hid/IOGPBus.h>
#include <driver/hw/m68k-amiga/floppy/FloppyController.h>
#include <driver/hw/m68k-amiga/graphics/GraphicsDriver.h>
#include <driver/hw/m68k-amiga/hid/AmiJoystick.h>
#include <driver/hw/m68k-amiga/hid/AmiKeyboard.h>
#include <driver/hw/m68k-amiga/hid/AmiLightPen.h>
#include <driver/hw/m68k-amiga/hid/AmiMouse.h>
#include <driver/hw/m68k-amiga/hid/AmiPaddle.h>
#include <driver/hw/m68k-amiga/zorro/ZorroController.h>
#include <driver/hw/m68k-amiga/zorro/ZorroDriver.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kpi/hid.h>
#include <kpi/smg.h>


final_class_ivars(AmigaController, PlatformController,
    ZorroControllerRef _Nonnull zorroController;
);

static errno_t _create_gpbus_hid_device(void* _Nullable ignore, int port, int type, DriverRef _Nullable * _Nonnull pOutDriver)
{
    switch (type) {
        case IOGP_MOUSE:
            return AmiMouse_Create(port, pOutDriver);

        case IOGP_LIGHTPEN:
            return AmiLightPen_Create(port, pOutDriver);

        case IOGP_ANALOG_JOYSTICK:
            return AmiPaddle_Create(port, pOutDriver);

        case IOGP_DIGITAL_JOYSTICK:
            return AmiJoystick_Create(port, pOutDriver);

        default:
            return EINVAL;
    }
}

// This function effectively "leaks" drivers when it fails. This doesn't matter
// because this platform controller never frees its children anyway.
errno_t AmigaController_detectDevices(struct AmigaController* _Nonnull _Locked self)
{
    decl_try_err();
    size_t slotId = 0;

    // Set our virtual bus slot count
    try(Driver_SetMaxChildCount((DriverRef)self, 8));


    // Graphics Driver
    GraphicsDriverRef fb = NULL;
    try(GraphicsDriver_Create(&fb));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)fb, slotId++));


    // Keyboard
    DriverRef kb;
    try(AmiKeyboard_Create(&kb));
    try(Driver_AttachStartChild((DriverRef)self, kb, slotId++));


    // GamePort
    IOGPBusRef gp = NULL;
    try(IOGPBus_Create(_create_gpbus_hid_device, NULL, &gp));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)gp, slotId++));


    // Floppy Bus
    FloppyControllerRef fdc = NULL;
    try(FloppyController_Create(&fdc));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)fdc, slotId++));


    // Zorro Bus
    try(ZorroController_Create(&self->zorroController));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)self->zorroController, slotId++));

catch:
    return err;
}

uint64_t AmigaController_getPhysicalMemorySize(struct AmigaController* _Nonnull self)
{
    uint64_t msize = 0;

    // Get the motherboard RAM
    msize += sys_desc_getramsize(g_sys_desc);


    // Look for RAM expansion boards o the Zorro bus. Only pick up RAM expansions
    // that were properly detected and where a ZRamDriver instance has been
    // assigned.
    const size_t nboards = Driver_GetChildCount((DriverRef)self->zorroController);
    for (size_t i = 0; i < nboards; i++) {
        DriverRef drv = Driver_GetChildAt((DriverRef)self->zorroController, i);
        const zorro_conf_t* cfg = ZorroDriver_GetConfiguration(drv);

        if (cfg->type == ZORRO_TYPE_RAM && Driver_GetChildCount(drv) > 0) {
            msize += cfg->logicalSize;
        }
    }

    return msize;
}

// Scans the ROM area following the end of the kernel looking for an embedded
// Serena disk image with a root filesystem.
const struct SMG_Header* _Nullable AmigaController_getBootImage(struct AmigaController* _Nonnull self)
{
    extern char _text, _etext, _data, _edata;
    const size_t txt_size = &_etext - &_text;
    const size_t dat_size = &_edata - &_data;
    const char* ps = (const char*)(BOOT_ROM_BASE + txt_size + dat_size);
    const uint32_t* pe4 = (const uint32_t*)__min((const char*)(BOOT_ROM_BASE + BOOT_ROM_SIZE), ps + CPU_PAGE_SIZE);
    const uint32_t* p4 = (const uint32_t*)__Ceil_Ptr_PowerOf2(ps, 4);
    const SMG_Header* smg_hdr = NULL;
    const uint32_t smg_sig = htobe32(SMG_SIGNATURE);

    while (p4 < pe4) {
        if (*p4 == smg_sig) {
            smg_hdr = (const SMG_Header*)p4;
            break;
        }

        p4++;
    }

    return smg_hdr;
}

class_func_defs(AmigaController, PlatformController,
override_func_def(detectDevices, AmigaController, PlatformController)
override_func_def(getPhysicalMemorySize, AmigaController, PlatformController)
override_func_def(getBootImage, AmigaController, PlatformController)
);
