//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
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
errno_t GraphicsDriver_Create(GraphicsDriverRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    GraphicsDriverRef self;
    
    try(Driver_Create(class(GraphicsDriver), 0, g_cats, (DriverRef*)&self));
    self->nextGObjId = 1;
    mtx_init(&self->io_mtx);


    // Create a null Copper program and null sprite
    copper_prog_t nullCopperProg;
    try(Surface_CreateNullSprite(&self->nullSpriteSurface));
    try(GraphicsDriver_CreateNullCopperProg(self, &nullCopperProg));
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
    try(Process_AcquireVirtualProcessor(gKernelProcess, (vcpu_func_t)GraphicsDriver_CopperManager, self, &attr, 0, &self->copvp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIGCOPRUN, self->copvp);

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

static errno_t GraphicsDriver_onStart(GraphicsDriverRef _Nonnull _Locked self)
{
    decl_try_err();

    CatalogEntry ce;
    ce.name = "fb";
    ce.resource = (ObjectRef)self;
    ce.func = IOGraphicsHandler_Create;
    ce.uid = UID_ROOT;
    ce.gid = GID_ROOT;
    ce.perms = fs_perms_from_octal(0666);

    err = Driver_Publish((DriverRef)self, &ce);
    if (err == EOK) {
        copper_start();
        vcpu_resume(self->copvp, false);
    }
    return err;
}


int _GraphicsDriver_GetNewGObjId(GraphicsDriverRef _Nonnull _Locked self)
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

void* _Nullable _GraphicsDriver_GetGObjForId(GraphicsDriverRef _Nonnull _Locked self, int id, int type)
{
    deque_for_each(&self->gobjs, GObject, it,
        if (GObject_GetId(it) == id) {
            return (GObject_GetType(it) == type) ? it : NULL;
        }
    )
    return NULL;
}

void _GraphicsDriver_DestroyGObj(GraphicsDriverRef _Nonnull _Locked self, void* gobj)
{
    deque_remove(&self->gobjs, GObject_GetChainPtr(gobj));
    GObject_DelRef(gobj);
}


class_func_defs(GraphicsDriver, DisplayDriver,
override_func_def(onStart, GraphicsDriver, Driver)
override_func_def(getScreenSize, GraphicsDriver, DisplayDriver)
override_func_def(setScreenConfigObserver, GraphicsDriver, DisplayDriver)
override_func_def(setLightPenEnabled, GraphicsDriver, DisplayDriver)
override_func_def(obtainCursor, GraphicsDriver, DisplayDriver)
override_func_def(releaseCursor, GraphicsDriver, DisplayDriver)
override_func_def(bindCursor, GraphicsDriver, DisplayDriver)
override_func_def(setCursorPosition, GraphicsDriver, DisplayDriver)
override_func_def(setCursorVisible, GraphicsDriver, DisplayDriver)
);
