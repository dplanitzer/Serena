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


VirtualDiskManagerRef    gVirtualDiskManager;


final_class_ivars(VirtualDiskManager, Driver,
);


errno_t VirtualDiskManager_Create(DriverRef _Nullable * _Nonnull pOutSelf)
{
    return Driver_Create(class(VirtualDiskManager), 0, NULL, pOutSelf);
}

errno_t VirtualDiskManager_onStart(DriverRef _Nonnull _Locked self)
{
    decl_try_err();

    BusEntry be;
    be.name = "vdisk";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = FilePermissions_MakeFromOctal(0755);

    DriverEntry de;
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = FilePermissions_MakeFromOctal(0666);
    de.arg = 0;

    try(Driver_PublishBus((DriverRef)self, &be, &de));

catch:
    return err;
}

errno_t VirtualDiskManager_CreateRamDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount)
{
    decl_try_err();
    DriverRef dd;

    try(RamDisk_Create((DriverRef)self, name, sectorSize, sectorCount, extentSectorCount, (RamDiskRef*)&dd));
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

    try(RomDisk_Create((DriverRef)self, name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dd));
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
