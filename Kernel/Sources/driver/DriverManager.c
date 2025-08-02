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


errno_t DriverManager_Publish(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver, const DriverEntry1* _Nonnull de)
{
    return Catalog_PublishDriver(gDriverCatalog, de->dirId, de->name, de->uid, de->gid, de->perms, driver, de->arg, &driver->driverCatalogId);
}

// Removes the driver instance from the driver catalog.
void DriverManager_Unpublish(DriverManagerRef _Nonnull self, DriverRef _Nonnull driver)
{
    if (driver->driverCatalogId != kCatalogId_None) {
        Catalog_Unpublish(gDriverCatalog, Driver_GetParentDirectoryId(driver), driver->driverCatalogId);
        driver->driverCatalogId = kCatalogId_None;
    }
}

errno_t DriverManager_CreateDirectory(DriverManagerRef _Nonnull self, const DirEntry* _Nonnull be, CatalogId* _Nonnull pOutDirId)
{
    return Catalog_PublishFolder(gDriverCatalog, be->dirId, be->name, be->uid, be->gid, be->perms, pOutDirId);
}

errno_t DriverManager_RemoveDirectory(DriverManagerRef _Nonnull self, CatalogId dirId)
{
    if (dirId != kCatalogId_None) {
        return Catalog_Unpublish(gDriverCatalog, dirId, kCatalogId_None);
    }
    else {
        return EOK;
    }
}
