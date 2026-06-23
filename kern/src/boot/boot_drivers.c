//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <driver/IOCatalog.h>
#include <driver/hid/HIDDriver.h>
#include <driver/pseudo/LogDriver.h>
#include <driver/pseudo/VDMDriver.h>
#include <handler/NullHandler.h>
#include <kpi/fs_perms.h>
#include <kpi/uid.h>
#ifdef MACHINE_AMIGA
#include <driver/hw/m68k-amiga/AmigaController.h>
#else
#error "unknown platform"
#endif


static Class* _Nonnull _get_platform_controller_class(void)
{
#ifdef MACHINE_AMIGA
    return class(AmigaController);
#else
    abort();
#endif
}

errno_t init_iokit(void)
{
    decl_try_err();
    DriverRef dp;

    // Create the I/O catalog
    try(IOCatalog_Create(&gIOCatalog));


    // Platform controller
    try(PlatformController_Create(_get_platform_controller_class(), &dp));
    try(Driver_Start(dp));
    gPlatformController = (PlatformControllerRef)dp;


    // 'hid' driver
    try(HIDDriver_Create(&dp));
    try(Driver_Start(dp));


    // 'klog' driver
    try(LogDriver_Create(&dp));
    try(Driver_Start(dp));

        
    // 'vdm' driver
    try(VDMDriver_Create(&dp));
    try(Driver_Start(dp));

catch:
    return err;
}


errno_t init_pseudo_devices(void)
{
    decl_try_err();
    CatalogEntry en;
    did_t dummy;

    en.name = "null";
    en.resource = NULL;
    en.context = NULL;
    en.func = NullHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0666);
    try(IOCatalog_PublishEntry(gIOCatalog, kCatalogId_None, &en, &dummy));

catch:
    return err;
}