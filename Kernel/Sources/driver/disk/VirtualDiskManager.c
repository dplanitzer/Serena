//
//  VirtualDiskManager.c
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "VirtualDiskManager.h"
#include "RamDisk.h"
#include "RomDisk.h"
#include <handler/HandlerChannel.h>
#include <driver/DriverManager.h>
#include <kpi/fcntl.h>

VirtualDiskManagerRef gVirtualDiskManager;


final_class_ivars(VirtualDiskManager, Handler,
    CatalogId   busDirId;
);


errno_t VirtualDiskManager_Create(VirtualDiskManagerRef _Nullable * _Nonnull pOutSelf)
{
    return Handler_Create(class(VirtualDiskManager), (HandlerRef*)pOutSelf);
}

errno_t VirtualDiskManager_Start(VirtualDiskManagerRef _Nonnull self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = 0;
    be.name = "vdisk";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(DriverManager_CreateDirectory(gDriverManager, &be, &self->busDirId));

    DriverEntry de;
    de.dirId = self->busDirId;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.category = 0;
    de.driver = (HandlerRef)self;
    de.arg = 0;

    try(DriverManager_Publish(gDriverManager, &de, NULL));

catch:
    if (err != EOK) {
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
    return err;
}

errno_t VirtualDiskManager_open(VirtualDiskManagerRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create((HandlerRef)self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t VirtualDiskManager_CreateRamDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount)
{
    decl_try_err();
    DriverRef dd;

    try(RamDisk_Create(self->busDirId, name, sectorSize, sectorCount, extentSectorCount, (RamDiskRef*)&dd));
    try(Driver_AdoptStartChild((DriverRef)self, dd));

    return EOK;

catch:
    if (dd) {
        Driver_Stop(dd, kDriverStop_Shutdown);
        Object_Release(dd);
    }
    return err;
}

errno_t VirtualDiskManager_CreateRomDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image)
{
    decl_try_err();
    DriverRef dd;

    try(RomDisk_Create(self->busDirId, name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dd));
    try(Driver_AdoptStartChild((DriverRef)self, dd));

    return EOK;

catch:
    if (dd) {
        Driver_Stop(dd, kDriverStop_Shutdown);
        Object_Release(dd);
    }
    return err;
}


class_func_defs(VirtualDiskManager, Handler,
override_func_def(open, VirtualDiskManager, Handler)
);
