//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "DriverCatalog.h"
#include <console/Console.h>
#include <dispatcher/Lock.h>
#include <driver/amiga/floppy/FloppyController.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include <driver/amiga/RealtimeClock.h>
#include <driver/hid/EventDriver.h>

typedef struct DriverManager {
    Lock                        lock;
    DriverCatalogRef _Nonnull   catalog;
    ExpansionBus                zorroBus;
    bool                        isZorroBusConfigured;
} DriverManager;

DriverManagerRef _Nonnull  gDriverManager;

const char* const kGraphicsDriverName = "graphics";
const char* const kConsoleName = "con";
const char* const kEventsDriverName = "events";
const char* const kRealtimeClockName = "rtc";
const char* const kFloppyDrive0Name = "fd0";



errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverManager* self;
    
    try(kalloc_cleared(sizeof(DriverManager), (void**) &self));
    Lock_Init(&self->lock);
    try(DriverCatalog_Create(&self->catalog));
    self->isZorroBusConfigured = false;
    self->zorroBus.board_count = 0;

    *pOutSelf = self;
    return EOK;

catch:
    DriverManager_Destroy(self);
    *pOutSelf = NULL;
    return err;
}

void DriverManager_Destroy(DriverManagerRef _Nullable self)
{
    if (self) {
        DriverCatalog_Destroy(self->catalog);
        Lock_Deinit(&self->lock);
        kfree(self);
    }
}

errno_t DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull self)
{
    decl_try_err();
    bool needsUnlock = false;

    Lock_Lock(&self->lock);
    needsUnlock = true;


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
    try(DriverCatalog_RegisterDriver(self->catalog, kGraphicsDriverName, pMainGDevice));


    // Event Driver
    EventDriverRef pEventDriver = NULL;
    try(EventDriver_Create(pMainGDevice, &pEventDriver));
    try(DriverCatalog_RegisterDriver(self->catalog, kEventsDriverName, pEventDriver));


    // Initialize the console
    ConsoleRef pConsole = NULL;
    try(Console_Create(pEventDriver, pMainGDevice, &pConsole));
    try(DriverCatalog_RegisterDriver(self->catalog, kConsoleName, pConsole));

    Lock_Unlock(&self->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

// Auto configures the expansion board bus.
static errno_t DriverManager_AutoConfigureExpansionBoardBus_Locked(DriverManagerRef _Nonnull self)
{
    if (self->isZorroBusConfigured) {
        return EOK;
    }


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


    // Done
    self->isZorroBusConfigured = true;
    return EOK;
}

errno_t DriverManager_AutoConfigure(DriverManagerRef _Nonnull self)
{
    decl_try_err();
    bool needsUnlock = false;

    Lock_Lock(&self->lock);
    needsUnlock = true;


    // Auto configure the expansion board bus
    try(DriverManager_AutoConfigureExpansionBoardBus_Locked(self));


    // Realtime Clock
    RealtimeClockRef pRealtimeClock = NULL;
    try(RealtimeClock_Create(gSystemDescription, &pRealtimeClock));
    try(DriverCatalog_RegisterDriver(self->catalog, kRealtimeClockName, pRealtimeClock));


    // Floppy
    FloppyControllerRef fdc = NULL;
    FloppyDiskRef fdx[MAX_FLOPPY_DISK_DRIVES];
    char fdx_name[4];

    fdx_name[0] = 'f';
    fdx_name[1] = 'd';
    fdx_name[2] = '\0';
    fdx_name[3] = '\0';

    try(FloppyController_Create(&fdc));
    try(FloppyController_DiscoverDrives(fdc, fdx));
    for(int i = 0; i < MAX_FLOPPY_DISK_DRIVES; i++) {
        if (fdx[i]) {
            fdx_name[2] = '0' + i;
            try(DriverCatalog_RegisterDriver(self->catalog, fdx_name, fdx[i]));
        }
    }


    Lock_Unlock(&self->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&self->lock);
    }
    return err;
}

DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull self, const char* name)
{
    return DriverCatalog_GetDriverForName(self->catalog, name);
}

int DriverManager_GetExpansionBoardCount(DriverManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const int count = self->zorroBus.board_count;
    Lock_Unlock(&self->lock);
    return count;
}

ExpansionBoard DriverManager_GetExpansionBoardAtIndex(DriverManagerRef _Nonnull self, int index)
{
    assert(index >= 0 && index < self->zorroBus.board_count);

    Lock_Lock(&self->lock);
    const ExpansionBoard board = self->zorroBus.board[index];
    Lock_Unlock(&self->lock);
    return board;
}
