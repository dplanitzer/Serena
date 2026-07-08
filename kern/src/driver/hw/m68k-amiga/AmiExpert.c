//
//  AmiExpert.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "AmiExpert.h"
#include <ext/endian.h>
#include <ext/math.h>
#include <driver/IORegistry.h>
#include <driver/hw/m68k-amiga/floppy/AFDBus.h>
#include <driver/hw/m68k-amiga/graphics/AGADriver.h>
#include <driver/hw/m68k-amiga/graphics/AmiHIDDisplay.h>
#include <driver/hw/m68k-amiga/hid/AmiGPBus.h>
#include <driver/hw/m68k-amiga/hid/AmiKeyboard.h>
#include <driver/hw/m68k-amiga/zorro/ZorroBus.h>
#include <driver/hw/m68k-amiga/zorro/ZorroDevice.h>
#include <driver/hw/m68k-amiga/zorro/ZRamDriver.h>
#include <hal/cpu.h>
#include <hal/sys_desc.h>
#include <hal/hw/m68k-amiga/chipset.h>
#include <kern/kalloc.h>
#include <kern/kernlib.h>
#include <kpi/hid.h>
#include <kpi/smg.h>

//
// Amiga platform resource usage table
//
// CIA timers:
// - CIA A, timer A: keyboard ack
// - CIA A, timer B: monotonic clock
// - CIA B, timer A: -
// - CIA B, timer B: -
//
// Interrupts:
//
// - Level 1, disk block:           AFDBus/AFDDevice
// - Level 2, CIA A, timer B:       monotonic clock timer services
// - Level 2, CIA A, serial port:   AmiKeyboard
// - Level 2, ports:                available for Zorro devices
// - Level 3, vertical blank:       AGADriver
// - Level 6, external:             available for Zorro devices
//
// DMA:
//
// - Bitplane DMA:                  AGADriver
// - Copper DMA:                    AGADriver
// - Disk DMA:                      AFDBus/AFDDevice
//


IOCATS_DEF(g_ram_cats, IOMEM_RAM);


final_class_ivars(AmiExpert, IOPlatformExpert,
);

static void _on_ram_exp_detected(struct AmiExpert* _Nonnull self, IODriverRef _Nonnull driver, int action)
{
    if (action != IOMATCH_STARTED) {
        return;
    }

    ZRamDriverRef zram = dynamiccast(driver, ZRamDriver);
    if (zram == NULL) {
        return;
    }
    if (IODriver_Open(zram, O_RDWR) != EOK) {
        return;
    }
    // We'll keep the RAM expansion driver open so that getPhysicalMemorySize()
    // below can get its size any time.


    mem_desc_t md = {0};

    md.lower = ZRamDriver_GetPhysicalBaseAddress(zram);
    md.upper = md.lower + ZRamDriver_GetMemorySize(zram);
    md.type = MEM_TYPE_MEMORY;
    (void) kalloc_add_memory_region(&md);
}

// This function effectively "leaks" drivers when it fails. This doesn't matter
// because this platform controller never frees its children anyway.
void AmiExpert_onLaunched(struct AmiExpert* _Nonnull self)
{
    decl_try_err();

    IORegistry_StartMatching(gIORegistry, g_ram_cats, (IOMatchCallback)_on_ram_exp_detected, self);


    // Graphics Driver
    AGADriverRef fb = NULL;
    try(AGADriver_Create(&fb));
    try(IODriver_Launch(fb, (IODriverRef)self));

    AmiHIDDisplayRef hid_disp = NULL;
    try(AmiHIDDisplay_Create(&hid_disp));
    try(IODriver_Launch(hid_disp, (IODriverRef)self));


    // Keyboard
    IODriverRef kb;
    try(AmiKeyboard_Create(&kb));
    try(IODriver_Launch(kb, (IODriverRef)self));


    // GamePort
    AmiGPBusRef gp = NULL;
    try(IOGPBus_Create(class(AmiGPBus), (IOGPBusRef*)&gp));
    try(IODriver_Launch(gp, (IODriverRef)self));


    // Floppy Bus
    AFDBusRef fdc = NULL;
    try(AFDBus_Create(&fdc));
    try(IODriver_Launch(fdc, (IODriverRef)self));


    // Zorro Bus
    ZorroBusRef zb = NULL;
    try(ZorroBus_Create(&zb));
    try(IODriver_Launch(zb, (IODriverRef)self));

    return;

catch:
    abort();
}

uint64_t AmiExpert_getPhysicalMemorySize(struct AmiExpert* _Nonnull self)
{
    decl_try_err();
    uint64_t msize = 0ull;
    IOIterator iter;

    // Get the motherboard RAM
    msize += sys_desc_getramsize(g_sys_desc);


    // Look for RAM expansion boards on the Zorro bus. Only pick up RAM expansions
    // that were properly detected and where a ZRamDriver instance has been
    // assigned.
    err = IORegistry_CopyMatchingDrivers(gIORegistry, g_ram_cats, &iter);
    if (err == EOK) {
        while (IOIterator_HasNext(&iter)) {
            ZRamDriverRef zram = dynamiccast(IOIterator_GetNext(&iter), ZRamDriver);

            if (zram) {
                msize += ZRamDriver_GetMemorySize(zram);
            }
        }
        IOIterator_Destroy(&iter);
    }

    return msize;
}

// Scans the ROM area following the end of the kernel looking for an embedded
// Serena disk image with a root filesystem.
const struct SMG_Header* _Nullable AmiExpert_getBootImage(struct AmiExpert* _Nonnull self)
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

class_func_defs(AmiExpert, IOPlatformExpert,
override_func_def(onLaunched, AmiExpert, IODriver)
override_func_def(getPhysicalMemorySize, AmiExpert, IOPlatformExpert)
override_func_def(getBootImage, AmiExpert, IOPlatformExpert)
);
