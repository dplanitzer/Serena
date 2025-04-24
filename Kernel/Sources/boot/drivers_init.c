//
//  drivers_init.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/4/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include <driver/DriverChannel.h>
#include <driver/LogDriver.h>
#include <driver/NullDriver.h>
#include <driver/amiga/AmigaController.h>
#include <driver/hid/HIDDriver.h>
#include <driver/hid/HIDManager.h>


// Creates and starts the platform controller which in turn discovers all platform
// specific drivers and gets them up and running.
errno_t drivers_init(void)
{
    static DriverRef gHidDriver;
    static DriverRef gLogDriver;
    static DriverRef gNullDriver;
    decl_try_err();

    // HID manager & driver
    try(HIDManager_Create(&gHIDManager));
    try(HIDDriver_Create(&gHidDriver));
    try(Driver_Start(gHidDriver));


    // Platform controller
    try(PlatformController_Create(class(AmigaController), &gPlatformController));
    try(Driver_Start(gPlatformController));


    // Start the HID manager
    try(HIDManager_Start(gHIDManager));


    // 'klog' driver
    try(LogDriver_Create(&gLogDriver));
    try(Driver_Start(gLogDriver));

        
    // 'null' driver
    try(NullDriver_Create(&gNullDriver));
    try(Driver_Start(gNullDriver));

catch:
    return err;
}
