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
#include <driver/hid/EventDriver.h>


final_class_ivars(AmigaController, PlatformController,
    ExpansionBus    zorroBus;
);


errno_t AmigaController_Create(PlatformControllerRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(AmigaController, kDriverModel_Sync, (DriverRef*)pOutSelf);
}

// Auto configures the expansion board bus.
static errno_t AmigaController_AutoConfigureExpansionBoardBus(struct AmigaController* _Nonnull self)
{
    // Auto config the Zorro bus
    zorro_auto_config(&self->zorroBus);


    // Find all RAM expansion boards and add them to the kalloc package
    for (int i = 0; i < self->zorroBus.board_count; i++) {
        const ExpansionBoard* board = &self->zorroBus.board[i];
       
        if (board->type == EXPANSION_TYPE_RAM && board->start != NULL && board->logical_size > 0) {
            MemoryDescriptor md = {0};

            md.lower = board->start;
            md.upper = board->start + board->logical_size;
            md.type = MEM_TYPE_MEMORY;
            (void) kalloc_add_memory_region(&md);
        }
    }

    return EOK;
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
    
    GraphicsDriverRef pMainGDevice = NULL;
    try(GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed3, &pMainGDevice));
    try(Driver_Start((DriverRef)pMainGDevice));
    try(DriverCatalog_RegisterDriver(gDriverCatalog, kGraphicsDriverName, (DriverRef)pMainGDevice));


    // Event Driver
    EventDriverRef pEventDriver = NULL;
    try(EventDriver_Create(pMainGDevice, &pEventDriver));
    try(Driver_Start((DriverRef)pEventDriver));
    try(DriverCatalog_RegisterDriver(gDriverCatalog, kEventsDriverName, (DriverRef)pEventDriver));


    // Initialize the console
    ConsoleRef pConsole = NULL;
    try(Console_Create(pEventDriver, pMainGDevice, &pConsole));
    try(Driver_Start((DriverRef)pConsole));
    try(DriverCatalog_RegisterDriver(gDriverCatalog, kConsoleName, (DriverRef)pConsole));


    // Let the kernel know that the console is now available
    PlatformController_NoteConsoleAvailable((PlatformControllerRef)self, pConsole);


    // Auto configure the expansion board bus
    try(AmigaController_AutoConfigureExpansionBoardBus(self));


    // Realtime Clock
    RealtimeClockRef pRealtimeClock = NULL;
    try(RealtimeClock_Create(gSystemDescription, &pRealtimeClock));
    try(Driver_Start((DriverRef)pRealtimeClock));
    try(DriverCatalog_RegisterDriver(gDriverCatalog, kRealtimeClockName, (DriverRef)pRealtimeClock));


    // Floppy
    FloppyControllerRef fdc = NULL;
    FloppyDiskRef fdx[MAX_FLOPPY_DISK_DRIVES];
    char fdx_name[4];

    fdx_name[0] = 'f';
    fdx_name[1] = 'd';
    fdx_name[2] = '\0';
    fdx_name[3] = '\0';

    try(FloppyController_Create(&fdc));
    try(Driver_Start((DriverRef)fdc));
    try(FloppyController_DiscoverDrives(fdc, fdx));
    for(int i = 0; i < MAX_FLOPPY_DISK_DRIVES; i++) {
        if (fdx[i]) {
            try(Driver_Start((DriverRef)fdx[i]));
            fdx_name[2] = '0' + i;
            try(DriverCatalog_RegisterDriver(gDriverCatalog, fdx_name, (DriverRef)fdx[i]));
        }
    }

catch:
    return err;
}

class_func_defs(AmigaController, PlatformController,
    override_func_def(start, AmigaController, Driver)
);
