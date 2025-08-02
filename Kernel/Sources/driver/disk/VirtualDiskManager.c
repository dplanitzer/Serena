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
#include <driver/DriverManager.h>


final_class_ivars(VirtualDiskManager, Driver,
    CatalogId   busDirId;
);


errno_t VirtualDiskManager_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(VirtualDiskManager), 0, NULL, kCatalogId_None, pOutSelf);
}

errno_t VirtualDiskManager_onStart(VirtualDiskManagerRef _Nonnull _Locked self)
{
    decl_try_err();

    DirEntry be;
    be.dirId = Driver_GetParentDirectoryId(self);
    be.name = "vdisk";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(DriverManager_CreateDirectory(gDriverManager, &be, &self->busDirId));
    ((DriverRef)self)->busCatalogId = self->busDirId;

    DriverEntry1 de;
    de.dirId = self->busDirId;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.arg = 0;

    try(DriverManager_Publish(gDriverManager, (DriverRef)self, &de));

catch:
    if (err != EOK) {
        DriverManager_RemoveDirectory(gDriverManager, self->busDirId);
    }
    return err;
}

errno_t VirtualDiskManager_CreateRamDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount)
{
    decl_try_err();
    DriverRef dd;

    try(RamDisk_Create((DriverRef)self, self->busDirId, name, sectorSize, sectorCount, extentSectorCount, (RamDiskRef*)&dd));
    try(Driver_StartAdoptChild((DriverRef)self, dd));

    return EOK;

catch:
    if (dd) {
        Driver_Terminate(dd);
        Object_Release(dd);
    }
    return err;
}

errno_t VirtualDiskManager_CreateRomDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image)
{
    decl_try_err();
    DriverRef dd;

    try(RomDisk_Create((DriverRef)self, self->busDirId, name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dd));
    try(Driver_StartAdoptChild((DriverRef)self, dd));

    return EOK;

catch:
    if (dd) {
        Driver_Terminate(dd);
        Object_Release(dd);
    }
    return err;
}


class_func_defs(VirtualDiskManager, Driver,
override_func_def(onStart, VirtualDiskManager, Driver)
);
