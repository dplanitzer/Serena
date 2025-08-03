//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <driver/DriverManager.h>
#include <driver/LogDriver.h>
#include <driver/NullDriver.h>
#include <driver/disk/VirtualDiskManager.h>
#include <driver/hid/HIDDriver.h>
#include <machine/amiga/AmigaController.h>


errno_t drivers_init(void)
{
    decl_try_err();

    // Platform controller
    try(PlatformController_Create(class(AmigaController), (DriverRef*)&gPlatformController));
    try(Driver_Start((DriverRef)gPlatformController));


    // 'hid' driver
    DriverRef hidDriver;
    try(HIDDriver_Create(&hidDriver));
    try(Driver_Start(hidDriver));


    // 'klog' driver
    DriverRef logDriver;
    try(LogDriver_Create(&logDriver));
    try(Driver_Start(logDriver));

        
    // 'null' driver
    DriverRef nullDriver;
    try(NullDriver_Create(&nullDriver));
    try(Driver_Start(nullDriver));


    // 'vdm' driver
    try(VirtualDiskManager_Create((DriverRef*)&gVirtualDiskManager));
    try(Driver_Start((DriverRef)gVirtualDiskManager));

catch:
    return err;
}
