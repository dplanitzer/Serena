//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include "copper.h"
#include <string.h>
#include <hal/irq.h>
#include <handler/IOGraphicsHandler.h>
#include <kern/kalloc.h>
#include <kpi/file.h>
#include <kpi/hid.h>
#include <process/kerneld.h>

IOCATS_DEF(g_cats, IOVID_FB);


// Creates a graphics driver instance which manages the on-board video hardware.
// We assume that video is turned off at the time this function is called and
// video remains turned off until a screen has been created and is made the
// current screen.
errno_t AGADriver_Create(AGADriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AGADriverRef self;
    
    try(IODriver_Create(class(AGADriver), g_cats, (IODriverRef*)&self));
    self->nextGObjId = 1;
    mtx_init(&self->io_mtx);


    // Create a null Copper program and null sprite
    copper_prog_t nullCopperProg;
    try(Surface_CreateNullSprite(&self->nullSpriteSurface));
    try(AGADriver_CreateNullCopperProg(self, &nullCopperProg));
    for (int i = 0; i < SPRITE_COUNT; i++) {
        self->spriteChannel[i].isVisible = true;
    }


    // Allocate the Copper management VCPU
    wq_init(&self->copvpWaitQueue);
    self->copvpSigs = sig_bit(SIGCOPRUN);

    vcpu_attr_t attr;
    attr.version = sizeof(vcpu_attr_t);
    attr.stack_size = 0;
    attr.group_id = VCPUID_MAIN_GROUP;
    attr.policy.version = sizeof(vcpu_policy_t);
    attr.policy.qos.grade = VCPU_QOS_URGENT;
    attr.policy.qos.priority = VCPU_PRI_NORMAL;
    attr.flags = 0;
    try(Process_AcquireVirtualProcessor(gKernelProcess, (vcpu_func_t)AGADriver_CopperManager, self, &attr, 0, &self->copvp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIGCOPRUN, self->copvp);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AGADriver_start(AGADriverRef _Nonnull self)
{
    copper_start();
    vcpu_resume(self->copvp, false);
    
    return EOK;
}

bool AGADriver_isExclusive(AGADriverRef _Nonnull self)
{
    return false;
}

errno_t AGADriver_getDFSInfo(AGADriverRef _Nonnull self, IODFSInfo* _Nonnull info)
{
    strcpy(info->name, "fb");
    info->func = IOGraphicsHandler_Create;
    info->uid = UID_ROOT;
    info->gid = GID_ROOT;
    info->perms = fs_perms_from_octal(0666);

    return EOK;
}


int _AGADriver_GetNewGObjId(AGADriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextGObjId++;

        deque_for_each(&self->gobjs, GObject, it,
            if (GObject_GetId(it) == id) {
                hasCollision = true;
                break;
            }
        )
    }
    return id;
}

void* _Nullable _AGADriver_GetGObjForId(AGADriverRef _Nonnull _Locked self, int id, int type)
{
    deque_for_each(&self->gobjs, GObject, it,
        if (GObject_GetId(it) == id) {
            return (GObject_GetType(it) == type) ? it : NULL;
        }
    )
    return NULL;
}

void _AGADriver_DestroyGObj(AGADriverRef _Nonnull _Locked self, void* gobj)
{
    deque_remove(&self->gobjs, GObject_GetChainPtr(gobj));
    GObject_DelRef(gobj);
}


class_func_defs(AGADriver, DisplayDriver,
override_func_def(start, AGADriver, IODriver)
override_func_def(isExclusive, AGADriver, IODriver)
override_func_def(getDFSInfo, AGADriver, IODriver)
override_func_def(getScreenSize, AGADriver, DisplayDriver)
override_func_def(setScreenConfigObserver, AGADriver, DisplayDriver)
override_func_def(setLightPenEnabled, AGADriver, DisplayDriver)
override_func_def(obtainCursor, AGADriver, DisplayDriver)
override_func_def(releaseCursor, AGADriver, DisplayDriver)
override_func_def(bindCursor, AGADriver, DisplayDriver)
override_func_def(setCursorPosition, AGADriver, DisplayDriver)
override_func_def(setCursorVisible, AGADriver, DisplayDriver)
);
