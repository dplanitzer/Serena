//
//  DriverManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/1/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DriverManager.h"
#include "LogDriver.h"
#include "NullDriver.h"
#include "disk/VirtualDiskManager.h"
#include "hid/HIDDriver.h"
#include "hid/HIDManager.h"
#include <kern/Kalloc.h>
#include <machine/amiga/AmigaController.h>
#include <Catalog.h>


DriverManagerRef gDriverManager;


errno_t DriverManager_Create(DriverManagerRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    DriverManagerRef self;

    try(kalloc_cleared(sizeof(DriverManager), (void**)&self));


catch:
    *pOutSelf = self;
    return err;
}

errno_t DriverManager_Start(DriverManagerRef _Nonnull self)
{
    decl_try_err();

    // Platform controller
    try(PlatformController_Create(class(AmigaController), &self->platformController));
    try(Driver_Start(self->platformController));


    // 'hid' driver
    try(HIDDriver_Create(&self->hidDriver));
    try(Driver_Start(self->hidDriver));


    // 'klog' driver
    try(LogDriver_Create(&self->logDriver));
    try(Driver_Start(self->logDriver));

        
    // 'null' driver
    try(NullDriver_Create(&self->nullDriver));
    try(Driver_Start(self->nullDriver));


    // 'vdm' driver
    try(VirtualDiskManager_Create(&self->virtualDiskDriver));
    try(Driver_Start(self->virtualDiskDriver));

catch:
    return err;
}

FilesystemRef _Nonnull DriverManager_GetCatalog(DriverManagerRef _Nonnull self)
{
    return Catalog_GetFilesystem(gDriverCatalog);
}

errno_t DriverManager_Open(DriverManagerRef _Nonnull self, const char* _Nonnull path, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return Catalog_Open(gDriverCatalog, path, mode, pOutChannel);
}

errno_t DriverManager_HasDriver(DriverManagerRef _Nonnull self, const char* _Nonnull path)
{
    return Catalog_IsPublished(gDriverCatalog, path);
}

errno_t DriverManager_AcquireNodeForPath(DriverManagerRef _Nonnull self, const char* _Nonnull path, ResolvedPath* _Nonnull rp)
{
    return Catalog_AcquireNodeForPath(gDriverCatalog, path, rp);
}
