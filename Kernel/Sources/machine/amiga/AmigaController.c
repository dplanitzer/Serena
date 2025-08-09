//
//  AmigaController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "AmigaController.h"
#include <filesystem/SerenaDiskImage.h>
#include <machine/cpu.h>
#include <machine/amiga/chipset.h>
#include <machine/amiga/floppy/FloppyController.h>
#include <machine/amiga/graphics/GraphicsDriver.h>
#include <machine/amiga/hid/GamePortController.h>
#include <machine/amiga/hid/KeyboardDriver.h>
#include <machine/amiga/zorro/ZorroController.h>
#include <kern/endian.h>


final_class_ivars(AmigaController, PlatformController,
);

errno_t AmigaController_detectDevices(struct AmigaController* _Nonnull _Locked self)
{
    decl_try_err();
    const CatalogId hwDirId = PlatformController_GetHardwareDirectoryId(self);
    
    // Set our virtual bus slot count
    try(Driver_SetMaxChildCount((DriverRef)self, 8));


    // Graphics Driver
    GraphicsDriverRef fb = NULL;
    try(GraphicsDriver_Create(hwDirId, &fb));
    try(Driver_StartAdoptChild((DriverRef)self, (DriverRef)fb));


    // Keyboard
    DriverRef kb;
    try(KeyboardDriver_Create(hwDirId, &kb));
    try(Driver_StartAdoptChild((DriverRef)self, kb));


    // GamePort
    GamePortControllerRef gpc = NULL;
    try(GamePortController_Create(hwDirId, &gpc));
    try(Driver_StartAdoptChild((DriverRef)self, (DriverRef)gpc));


    // Floppy Bus
    FloppyControllerRef fdc = NULL;
    try(FloppyController_Create(hwDirId, &fdc));
    try(Driver_StartAdoptChild((DriverRef)self, (DriverRef)fdc));


    // Zorro Bus
    ZorroControllerRef zorroController = NULL;
    try(ZorroController_Create(hwDirId, &zorroController));
    try(Driver_StartAdoptChild((DriverRef)self, (DriverRef)zorroController));

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
