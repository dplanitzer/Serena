//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include <console/Console.h>
#include <dispatcher/Lock.h>
#include <driver/amiga/cbm-floppy/FloppyDisk.h>
#include <driver/amiga/cbm-graphics/GraphicsDriver.h>
#include <driver/amiga/RealtimeClock.h>
#include <driver/hid/EventDriver.h>


const char* const kGraphicsDriverName = "graphics";
const char* const kConsoleName = "con";
const char* const kEventsDriverName = "events";
const char* const kRealtimeClockName = "rtc";
const char* const kFloppyDrive0Name = "fd0";


typedef struct DriverEntry {
    SListNode   node;
    const char* name;
    DriverRef   instance;
} DriverEntry;


typedef struct DriverManager {
    Lock            lock;
    SList           drivers;
    ExpansionBus    zorroBus;
    bool            isZorroBusConfigured;
} DriverManager;


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DriverEntry

static void DriverEntry_Destroy(DriverEntry* _Nullable pEntry)
{
    if (pEntry) {
        SListNode_Deinit(&pEntry->node);
        pEntry->name = NULL;

        Object_Release(pEntry->instance);
        pEntry->instance = NULL;
        
        kfree(pEntry);
    }
}

// Adopts the given driver. Meaning that this initializer does not take out an
// extra strong reference to the driver. It assumes that it should take ownership
// of the provided strong reference.
static errno_t DriverEntry_Create(const char* _Nonnull pName, DriverRef _Nonnull pDriverInstance, DriverEntry* _Nullable * _Nonnull pOutEntry)
{
    decl_try_err();
    DriverEntry* pEntry = NULL;

    try(kalloc_cleared(sizeof(DriverEntry), (void**) &pEntry));
    SListNode_Init(&pEntry->node);
    pEntry->name = pName;
    pEntry->instance = pDriverInstance;

    *pOutEntry = pEntry;
    return EOK;

catch:
    DriverEntry_Destroy(pEntry);
    *pOutEntry = NULL;
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: DriverManager

DriverManagerRef _Nonnull  gDriverManager;


errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    DriverManager* pManager;
    
    try(kalloc_cleared(sizeof(DriverManager), (void**) &pManager));
    Lock_Init(&pManager->lock);
    SList_Init(&pManager->drivers);
    pManager->isZorroBusConfigured = false;
    pManager->zorroBus.board_count = 0;

    *pOutManager = pManager;
    return EOK;

catch:
    DriverManager_Destroy(pManager);
    *pOutManager = NULL;
    return err;
}

void DriverManager_Destroy(DriverManagerRef _Nullable pManager)
{
    if (pManager) {
        SListNode* pCurEntry = pManager->drivers.first;

        while (pCurEntry != NULL) {
            SListNode* pNextEntry = pCurEntry->next;

            DriverEntry_Destroy((DriverEntry*)pCurEntry);
            pCurEntry = pNextEntry;
        }
        
        SList_Deinit(&pManager->drivers);
        Lock_Deinit(&pManager->lock);
        kfree(pManager);
    }
}

static errno_t DriverManager_AddDriver_Locked(DriverManagerRef _Nonnull pManager, const char* _Nonnull pName, DriverRef _Nonnull pDriverInstance)
{
    decl_try_err();
    DriverEntry* pEntry = NULL;

    try(DriverEntry_Create(pName, pDriverInstance, &pEntry));
    SList_InsertAfterLast(&pManager->drivers, &pEntry->node);
    return EOK;

catch:
    return err;
}

static DriverRef DriverManager_GetDriverForName_Locked(DriverManagerRef _Nonnull pManager, const char* pName)
{
    SList_ForEach(&pManager->drivers, DriverEntry, {
        if (String_Equals(pCurNode->name, pName)) {
            return pCurNode->instance;
        }
    })

    return NULL;
}


errno_t DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull pManager)
{
    decl_try_err();
    bool needsUnlock = false;

    Lock_Lock(&pManager->lock);
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
    try(DriverManager_AddDriver_Locked(pManager, kGraphicsDriverName, pMainGDevice));


    // Event Driver
    EventDriverRef pEventDriver = NULL;
    try(EventDriver_Create(pMainGDevice, &pEventDriver));
    try(DriverManager_AddDriver_Locked(pManager, kEventsDriverName, pEventDriver));


    // Initialize the console
    ConsoleRef pConsole = NULL;
    try(Console_Create(pEventDriver, pMainGDevice, &pConsole));
    try(DriverManager_AddDriver_Locked(pManager, kConsoleName, pConsole));

    Lock_Unlock(&pManager->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pManager->lock);
    }
    return err;
}

// Auto configures the expansion board bus.
static errno_t DriverManager_AutoConfigureExpansionBoardBus_Locked(DriverManagerRef _Nonnull pManager)
{
    if (pManager->isZorroBusConfigured) {
        return EOK;
    }


    // Auto config the Zorro bus
    zorro_auto_config(&pManager->zorroBus);


    // Find all RAM expansion boards and add them to the kalloc package
    for (int i = 0; i < pManager->zorroBus.board_count; i++) {
        const ExpansionBoard* board = &pManager->zorroBus.board[i];
       
        if (board->type == EXPANSION_TYPE_RAM && board->start != NULL && board->logical_size > 0) {
            MemoryDescriptor md = {0};

            md.lower = board->start;
            md.upper = board->start + board->logical_size;
            md.type = MEM_TYPE_MEMORY;
            (void) kalloc_add_memory_region(&md);
        }
    }


    // Done
    pManager->isZorroBusConfigured = true;
    return EOK;
}

errno_t DriverManager_AutoConfigure(DriverManagerRef _Nonnull pManager)
{
    decl_try_err();
    bool needsUnlock = false;

    Lock_Lock(&pManager->lock);
    needsUnlock = true;


    // Auto configure the expansion board bus
    try(DriverManager_AutoConfigureExpansionBoardBus_Locked(pManager));


    // Realtime Clock
    RealtimeClockRef pRealtimeClock = NULL;
    try(RealtimeClock_Create(gSystemDescription, &pRealtimeClock));
    try(DriverManager_AddDriver_Locked(pManager, kRealtimeClockName, pRealtimeClock));


    // Floppy
#ifndef __BOOT_FROM_ROM__
    // XXX Stops A4000 (68040/68060) from completing the boot process, if enabled
    FloppyDiskRef fd0 = NULL;
    try(FloppyDMA_Create(&gFloppyDma));
    try(FloppyDisk_Create(0, &fd0));
    try(FloppyDisk_Reset(fd0));
    try(DriverManager_AddDriver_Locked(pManager, kFloppyDrive0Name, fd0));
#endif


    Lock_Unlock(&pManager->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pManager->lock);
    }
    return err;
}

DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull pManager, const char* pName)
{
    Lock_Lock(&pManager->lock);
    DriverRef pDriver = DriverManager_GetDriverForName_Locked(pManager, pName);
    Lock_Unlock(&pManager->lock);
    return pDriver;
}

int DriverManager_GetExpansionBoardCount(DriverManagerRef _Nonnull pManager)
{
    Lock_Lock(&pManager->lock);
    const int count = pManager->zorroBus.board_count;
    Lock_Unlock(&pManager->lock);
    return count;
}

ExpansionBoard DriverManager_GetExpansionBoardAtIndex(DriverManagerRef _Nonnull pManager, int index)
{
    assert(index >= 0 && index < pManager->zorroBus.board_count);

    Lock_Lock(&pManager->lock);
    const ExpansionBoard board = pManager->zorroBus.board[index];
    Lock_Unlock(&pManager->lock);
    return board;
}
