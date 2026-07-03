//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <console/Console.h>
#include <driver/IORegistry.h>
#include <driver/hid/IOHIDManager.h>
#include <handler/ConsoleHandler.h>
#include <handler/IOHIDHandler.h>
#include <handler/LogHandler.h>
#include <handler/NullHandler.h>
#include <kern/devfs.h>
#include <kpi/fs_perms.h>
#include <kpi/uid.h>
#ifdef MACHINE_AMIGA
#include <driver/hw/m68k-amiga/AmiExpert.h>
#else
#error "unknown platform"
#endif


errno_t init_iokit(void)
{
    decl_try_err();
    IODriverRef dp;
    Class* pcls;

    // Create devfs and the I/O registry
    try(devfs_init());
    IORegistry_Init();


#ifdef MACHINE_AMIGA
    pcls = class(AmiExpert);
#else
    abort();
#endif

    // Platform expert
    try(IOPlatformExpert_Create(pcls, &dp));
    try(IODriver_Launch(dp, NULL));
    gIOPlatformExpert = (IOPlatformExpertRef)dp;

catch:
    return err;
}


errno_t init_pseudo_devices(void)
{
    decl_try_err();
    devfs_entry_t en;

    en.name = "null";
    en.resource = NULL;
    en.func = NullHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0666);
    try(devfs_add(&en, NULL));


    en.name = "klog";
    en.resource = NULL;
    en.func = LogHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0440);
    try(devfs_add(&en, NULL));


    try(IOHIDManager_Create(&gIOHIDManager));
    try(IOHIDManager_Start(gIOHIDManager));

    en.name = "hid";
    en.resource = NULL;
    en.func = IOHIDHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0666);
    try(devfs_add(&en, NULL));

catch:
    return err;
}

errno_t init_console(void)
{
    decl_try_err();
    devfs_entry_t en;

    try(Console_Create(&gConsole));
    Console_Start(gConsole);

    en.name = "console";
    en.resource = NULL;
    en.func = ConsoleHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0666);
    try(devfs_add(&en, NULL));

catch:
    return err;
}
