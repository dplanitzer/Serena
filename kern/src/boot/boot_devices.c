//
//  boot_drivers.c
//  kernel
//
//  Created by Dietmar Planitzer on 8/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include <console/Console.h>
#include <driver/hid/IOHIDManager.h>
#include <driver/IOLib.h>
#include <handler/ConsoleHandler.h>
#include <handler/IOHIDHandler.h>
#include <handler/LogHandler.h>
#include <handler/NullHandler.h>
#include <kern/devfs.h>
#include <kpi/fs_perms.h>
#include <kpi/uid.h>
#include <sched/sem.h>


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

static void _init_console(sem_t* _Nonnull sem)
{
    devfs_entry_t en;

    try_bang(Console_Create(&gConsole));
    Console_Start(gConsole);

    en.name = "console";
    en.resource = NULL;
    en.func = ConsoleHandler_Create;
    en.uid = UID_ROOT;
    en.gid = GID_ROOT;
    en.perms = fs_perms_from_octal(0666);
    try_bang(devfs_add(&en, NULL));

    sem_post(sem);
}

errno_t init_console(void)
{
    decl_try_err();
    vcpu_t vp = NULL;
    sem_t sem;

    // We do this because the graphic driver enforces command buffer, image and sprite
    // ownership and the console is really a shared resource, yet owned by the kernel
    // (the console command buffer & sprites belong to the kernel but are used on
    // behalf of some user space app). The console will be moved to user space in the
    // future - where it belongs really
    sem_init(&sem, 0);
    err = IOAcquireVirtualProcessor((vcpu_func_t)_init_console, &sem, VCPU_QOS_URGENT, VCPU_PRI_NORMAL, &vp);
    if (err == EOK) {
        IOResumeVirtualProcessor(vp);
        sem_wait(&sem);
    }
    sem_deinit(&sem);

    return err;
}