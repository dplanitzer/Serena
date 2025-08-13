//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <driver/DriverManager.h>
#include <driver/disk/VirtualDiskManager.h>
#include <handler/HIDHandler.h>
#include <handler/LogHandler.h>
#include <handler/NullHandler.h>
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

    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;


    // Platform controller
    try(PlatformController_Create(_get_platform_controller_class(), (DriverRef*)&gPlatformController));
    try(Driver_Start((DriverRef)gPlatformController));


    // 'hid' driver
    de.name = "hid";
    de.perms = perm_from_octal(0666);
    try(HIDHandler_Create(&de.driver));
    try(DriverManager_Publish(gDriverManager, &de, NULL));


    // 'klog' driver
    de.name = "klog";
    de.perms = perm_from_octal(0440);
    try(LogHandler_Create(&de.driver));
    try(DriverManager_Publish(gDriverManager, &de, NULL));

        
    // 'null' driver
    de.name = "null";
    de.perms = perm_from_octal(0666);
    try(NullHandler_Create(&de.driver));
    try(DriverManager_Publish(gDriverManager, &de, NULL));


    // 'vdm' driver
    try(VirtualDiskManager_Create(&gVirtualDiskManager));
    try(VirtualDiskManager_Start(gVirtualDiskManager));

catch:
    return err;
}
