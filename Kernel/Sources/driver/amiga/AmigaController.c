//
//  AmigaController.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "AmigaController.h"
#include "DriverCatalog.h"
#include <console/Console.h>
#include <dispatcher/Lock.h>
#include <driver/amiga/floppy/FloppyController.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/amiga/RealtimeClock.h>
#include <driver/amiga/zorro/ZorroController.h>
#include <driver/hid/EventDriver.h>


final_class_ivars(AmigaController, PlatformController,
);


errno_t AmigaController_Create(PlatformControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(AmigaController, kDriverModel_Sync, (DriverRef*)pOutSelf);
}

errno_t AmigaController_start(struct AmigaController* _Nonnull self)
{
    decl_try_err();


    // Graphics Driver
    const ScreenConfiguration* pVideoConfig;
    if (chipset_is_ntsc()) {
        pVideoConfig = &kScreenConfig_NTSC_640_200_60;
        //pVideoConfig = &kScreenConfig_NTSC_640_400_30;
    } else {
        pVideoConfig = &kScreenConfig_PAL_640_256_50;
        //pVideoConfig = &kScreenConfig_PAL_640_512_25;
    }
    
    GraphicsDriverRef graphDriver = NULL;
    try(GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed3, &graphDriver));
    try(Driver_Start((DriverRef)graphDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)graphDriver);


    // Event Driver
    EventDriverRef eventDriver = NULL;
    try(EventDriver_Create(graphDriver, &eventDriver));
    try(Driver_Start((DriverRef)eventDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)eventDriver);


    // Initialize the console
    ConsoleRef console = NULL;
    try(Console_Create(eventDriver, graphDriver, &console));
    try(Driver_Start((DriverRef)console));
    Driver_AdoptChild((DriverRef)self, (DriverRef)console);


    // Let the kernel know that the console is now available
    PlatformController_NoteConsoleAvailable((PlatformControllerRef)self, console);


    // Floppy Bus
    FloppyControllerRef fdc = NULL;
    try(FloppyController_Create(&fdc));
    try(Driver_Start((DriverRef)fdc));
    Driver_AdoptChild((DriverRef)self, (DriverRef)fdc);


    // Realtime Clock
    RealtimeClockRef rtcDriver = NULL;
    try(RealtimeClock_Create(gSystemDescription, &rtcDriver));
    try(Driver_Start((DriverRef)rtcDriver));
    Driver_AdoptChild((DriverRef)self, (DriverRef)rtcDriver);


    // Zorro Bus
    ZorroControllerRef zorroController = NULL;
    try(ZorroController_Create(&zorroController));
    try(Driver_Start((DriverRef)zorroController));
    Driver_AdoptChild((DriverRef)self, (DriverRef)zorroController);

catch:
    return err;
}

class_func_defs(AmigaController, PlatformController,
override_func_def(start, AmigaController, Driver)
);
