//
//  DriverManager.c
//  Apollo
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include <console/Console.h>
#include "EventDriver.h"
#include "FloppyDisk.h"
#include "GraphicsDriver.h"
#include "Lock.h"
#include "RealtimeClock.h"


const Character* const kGraphicsDriverName = "graphics";
const Character* const kConsoleName = "con";
const Character* const kEventsDriverName = "events";
const Character* const kRealtimeClockName = "rtc";


typedef struct _DriverEntry {
    SListNode           node;
    const Character*    name;
    DriverRef           instance;
} DriverEntry;


typedef struct _DriverManager {
    Lock            lock;
    SList           drivers;
    ExpansionBus    zorroBus;
    Bool            isZorroBusConfigured;
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
        
        kfree((Byte*)pEntry);
    }
}

// Adopts the given driver. Meaning that this initializer does not take out an
// extra strong reference to the driver. It assumes that it should take ownership
// of the provided strong reference.
static ErrorCode DriverEntry_Create(const Character* _Nonnull pName, DriverRef _Nonnull pDriverInstance, DriverEntry* _Nullable * _Nonnull pOutEntry)
{
    decl_try_err();
    DriverEntry* pEntry = NULL;

    try(kalloc_cleared(sizeof(DriverEntry), (Byte**)&pEntry));
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


ErrorCode DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutManager)
{
    decl_try_err();
    DriverManager* pManager;
    
    try(kalloc_cleared(sizeof(DriverManager), (Byte**) &pManager));
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
        kfree((Byte*)pManager);
    }
}

static ErrorCode DriverManager_AddDriver_Locked(DriverManagerRef _Nonnull pManager, const Character* _Nonnull pName, DriverRef _Nonnull pDriverInstance)
{
    decl_try_err();
    DriverEntry* pEntry = NULL;

    try(DriverEntry_Create(pName, pDriverInstance, &pEntry));
    SList_InsertAfterLast(&pManager->drivers, &pEntry->node);
    return EOK;

catch:
    return err;
}

static DriverRef DriverManager_GetDriverForName_Locked(DriverManagerRef _Nonnull pManager, const Character* pName)
{
    SList_ForEach(&pManager->drivers, DriverEntry, {
        if (String_Equals(pCurNode->name, pName)) {
            return pCurNode->instance;
        }
    })

    return NULL;
}


ErrorCode DriverManager_AutoConfigureForConsole(DriverManagerRef _Nonnull pManager)
{
    decl_try_err();
    Bool needsUnlock = false;

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
    try(GraphicsDriver_Create(pVideoConfig, kPixelFormat_RGB_Indexed1, &pMainGDevice));
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
static ErrorCode DriverManager_AutoConfigureExpansionBoardBus_Locked(DriverManagerRef _Nonnull pManager)
{
    if (pManager->isZorroBusConfigured) {
        return EOK;
    }


    // Auto config the Zorro bus
    zorro_auto_config(&pManager->zorroBus);

    
    // Find all RAM expansion boards and do a mem check on them
    MemoryLayout mem_layout;
    mem_layout.descriptor_count = 0;

    for (Int i = 0; i < pManager->zorroBus.board_count; i++) {
        const ExpansionBoard* board = &pManager->zorroBus.board[i];
       
        if (board->type != EXPANSION_TYPE_RAM) {
            continue;
        }
        
        if (!mem_check_region(&mem_layout, board->start, board->start + board->logical_size, MEM_TYPE_MEMORY)) {
            break;
        }
    }


    // Add the RAM we found to the kernel allocator
    for (Int i = 0; i < mem_layout.descriptor_count; i++) {
        (void) kalloc_add_memory_region(&mem_layout.descriptor[i]);
    }


    // Done
    pManager->isZorroBusConfigured = true;
}

ErrorCode DriverManager_AutoConfigure(DriverManagerRef _Nonnull pManager)
{
    decl_try_err();
    Bool needsUnlock = false;

    Lock_Lock(&pManager->lock);
    needsUnlock = true;


    // Auto configure the expansion board bus
    try(DriverManager_AutoConfigureExpansionBoardBus_Locked(pManager));


    // Realtime Clock
    RealtimeClockRef pRealtimeClock = NULL;
    try(RealtimeClock_Create(gSystemDescription, &pRealtimeClock));
    try(DriverManager_AddDriver_Locked(pManager, kRealtimeClockName, pRealtimeClock));


    // Floppy
//    FloppyDMA* pFloppyDma = NULL;
//    try(FloppyDMA_Create(&pFloppyDma));
//    try(DriverManager_AddDriver_Locked(pManager, "fdma", pFloppyDma));


    Lock_Unlock(&pManager->lock);
    return EOK;

catch:
    if (needsUnlock) {
        Lock_Unlock(&pManager->lock);
    }
    return err;
}

DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull pManager, const Character* pName)
{
    Lock_Lock(&pManager->lock);
    DriverRef pDriver = DriverManager_GetDriverForName_Locked(pManager, pName);
    Lock_Unlock(&pManager->lock);
    return pDriver;
}

Int DriverManager_GetExpansionBoardCount(DriverManagerRef _Nonnull pManager)
{
    Lock_Lock(&pManager->lock);
    const Int count = pManager->zorroBus.board_count;
    Lock_Unlock(&pManager->lock);
    return count;
}

ExpansionBoard DriverManager_GetExpansionBoardAtIndex(DriverManagerRef _Nonnull pManager, Int index)
{
    assert(index >= 0 && index < pManager->zorroBus.board_count);

    Lock_Lock(&pManager->lock);
    const ExpansionBoard board = pManager->zorroBus.board[index];
    Lock_Unlock(&pManager->lock);
    return board;
}
