//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <driver/DriverManager.h>
#include <driver/disk/VirtualDiskManager.h>
#include <driver/pseudo/HIDDriver.h>
#include <driver/pseudo/LogDriver.h>
#include <driver/pseudo/NullDriver.h>
#ifdef MACHINE_AMIGA
#include <machine/amiga/AmigaController.h>
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

errno_t drivers_init(void)
{
    decl_try_err();
    DriverEntry de = (DriverEntry){0};
    DriverRef dp;

    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;


    // Platform controller
    try(PlatformController_Create(_get_platform_controller_class(), (DriverRef*)&gPlatformController));
    try(Driver_Start((DriverRef)gPlatformController));


    // 'hid' driver
    try(HIDDriver_Create(&dp));
    try(Driver_Start(dp));


    // 'klog' driver
    try(LogDriver_Create(&dp));
    try(Driver_Start(dp));

        
    // 'null' driver
    try(NullDriver_Create(&dp));
    try(Driver_Start(dp));


    // 'vdm' driver
    try(VirtualDiskManager_Create(&gVirtualDiskManager));
    try(VirtualDiskManager_Start(gVirtualDiskManager));

catch:
    return err;
}
