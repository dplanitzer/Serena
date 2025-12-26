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
#include <filesystem/SerenaDiskImage.h>
#include <driver/hw/m68k-amiga/floppy/FloppyController.h>
#include <driver/hw/m68k-amiga/graphics/GraphicsDriver.h>
#include <driver/hw/m68k-amiga/hid/GamePortController.h>
#include <driver/hw/m68k-amiga/hid/KeyboardDriver.h>
#include <driver/hw/m68k-amiga/zorro/ZorroController.h>
#include <hal/cpu.h>
#include <hal/hw/m68k-amiga/chipset.h>


final_class_ivars(AmigaController, PlatformController,
);

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
    try(KeyboardDriver_Create(&kb));
    try(Driver_AttachStartChild((DriverRef)self, kb, slotId++));


    // GamePort
    GamePortControllerRef gpc = NULL;
    try(GamePortController_Create(&gpc));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)gpc, slotId++));


    // Floppy Bus
    FloppyControllerRef fdc = NULL;
    try(FloppyController_Create(&fdc));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)fdc, slotId++));


    // Zorro Bus
    ZorroControllerRef zorroController = NULL;
    try(ZorroController_Create(&zorroController));
    try(Driver_AttachStartChild((DriverRef)self, (DriverRef)zorroController, slotId++));

catch:
    return err;
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
    const uint32_t smg_sig = UInt32_HostToBig(SMG_SIGNATURE);

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
override_func_def(getBootImage, AmigaController, PlatformController)
);
