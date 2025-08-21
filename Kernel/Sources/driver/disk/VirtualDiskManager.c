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
#include <kpi/fcntl.h>

VirtualDiskManagerRef gVirtualDiskManager;


final_class_ivars(VirtualDiskManager, Handler,
);


errno_t VirtualDiskManager_Create(VirtualDiskManagerRef _Nullable * _Nonnull pOutSelf)
{
    return Handler_Create(class(VirtualDiskManager), (HandlerRef*)pOutSelf);
}

errno_t VirtualDiskManager_Start(VirtualDiskManagerRef _Nonnull self)
{
    decl_try_err();
#if 0
    //XXX bring back
    DirEntry be;
    be.dirId = 0;
    be.name = "vdisk";
    be.uid = kUserId_Root;
    be.gid = kGroupId_Root;
    be.perms = perm_from_octal(0755);

    try(Driver_PublishBusDirectory((DriverRef)self, &be));

    DriverEntry de;
    de.dirId = Driver_GetPublishedBusDirectory(self);
    de.name = "self";
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
    de.driver = (HandlerRef)self;
    de.arg = 0;

    try(Driver_Publish(self, &de));

catch:
    if (err != EOK) {
        Driver_UnpublishBusDirectory((DriverRef)self);
    }
#endif
    return err;
}

errno_t VirtualDiskManager_open(VirtualDiskManagerRef _Nonnull self, unsigned int mode, intptr_t arg, IOChannelRef _Nullable * _Nonnull pOutChannel)
{
    return HandlerChannel_Create((HandlerRef)self, SEO_FT_DRIVER, mode, 0, pOutChannel);
}

errno_t VirtualDiskManager_CreateRamDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount)
{
    decl_try_err();
    DriverRef dp;

    try(RamDisk_Create(name, sectorSize, sectorCount, extentSectorCount, (RamDiskRef*)&dp));
    try(Driver_AttachStartChild((DriverRef)self, dp, 0));

catch:
    Object_Release(dp);
    return err;
}

errno_t VirtualDiskManager_CreateRomDisk(VirtualDiskManagerRef _Nonnull self, const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, const void* _Nonnull image)
{
    decl_try_err();
    DriverRef dp;

    try(RomDisk_Create(name, image, sectorSize, sectorCount, false, (RomDiskRef*)&dp));
    try(Driver_AttachStartChild((DriverRef)self, dp, 0));

catch:
    Object_Release(dp);
    return err;
}


class_func_defs(VirtualDiskManager, Handler,
override_func_def(open, VirtualDiskManager, Handler)
);
