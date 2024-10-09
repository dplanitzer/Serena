//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "DriverCatalog.h"
#include <dispatcher/Lock.h>
#include <driver/amiga/AmigaController.h>


typedef struct DriverManager {
    Lock                            lock;
    PlatformControllerRef _Nonnull  platformController;
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
    try(AmigaController_Create(&self->platformController));
    try(Driver_Start((DriverRef)self->platformController));

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
        Object_Release(self->platformController);
        self->platformController = NULL;

        Lock_Deinit(&self->lock);
        kfree(self);
    }
}

errno_t DriverManager_Start(DriverManagerRef _Nonnull self)
{
    Lock_Lock(&self->lock);
    const errno_t err = Driver_Start((DriverRef)self->platformController);
    Lock_Unlock(&self->lock);
    return err;
}

DriverRef DriverManager_GetDriverForName(DriverManagerRef _Nonnull self, const char* name)
{
    return DriverCatalog_GetDriverForName(gDriverCatalog, name);
}
