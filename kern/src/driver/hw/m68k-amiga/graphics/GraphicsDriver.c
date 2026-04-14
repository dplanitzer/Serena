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
#include <kern/kalloc.h>
#include <kpi/file.h>
#include <kpi/hid.h>

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

    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)GraphicsDriver_CopperManager;
    attr.arg = self;
    attr.stack_size = 0;
    attr.group_id = VCPUID_MAIN_GROUP;
    attr.policy.version = sizeof(vcpu_policy_t);
    attr.policy.qos.grade = VCPU_QOS_URGENT;
    attr.policy.qos.priority = VCPU_PRI_NORMAL;
    attr.flags = 0;
    attr.data = 0;
    try(Process_AcquireVirtualProcessor(gKernelProcess, &attr, &self->copvp));

    
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

    DriverEntry de;
    de.name = "fb";
    de.uid = UID_ROOT;
    de.gid = GID_ROOT;
    de.perms = fs_perms_from_octal(0666);
    de.arg = 0;

    err = Driver_Publish((DriverRef)self, &de);
    if (err == EOK) {
        copper_start();
        vcpu_resume(self->copvp, false);
    }
    return err;
}

errno_t GraphicsDriver_ioctl(GraphicsDriverRef _Nonnull self, IOChannelRef _Nonnull pChannel, int cmd, va_list ap)
{
    switch (cmd) {
        case kFBCommand_CreateSurface2d: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const pixfmt_t fmt = va_arg(ap, pixfmt_t);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateSurface2d(self, width, height, fmt, hnd);
        }

        case kFBCommand_DestroySurface: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroySurface(self, hnd);
        }

        case kFBCommand_GetSurfaceInfo: {
            int hnd = va_arg(ap, int);
            surface_info_t* si = va_arg(ap, surface_info_t*);

            return GraphicsDriver_GetSurfaceInfo(self, hnd, si);
        }

        case kFBCommand_MapSurface: {
            int hnd = va_arg(ap, int);
            int mode = va_arg(ap, int);
            surface_mapping_t* sm = va_arg(ap, surface_mapping_t*);

            return GraphicsDriver_MapSurface(self, hnd, mode, sm);
        }

        case kFBCommand_UnmapSurface: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_UnmapSurface(self, hnd);
        }

        case kFBCommand_WritePixels: {
            int hnd = va_arg(ap, int);
            const void* planes = va_arg(ap, const void*);
            size_t bytesPerRow = va_arg(ap, size_t);
            pixfmt_t format = va_arg(ap, pixfmt_t);

            return GraphicsDriver_WritePixels(self, hnd, planes, bytesPerRow, format);
        }

        case kFBCommand_ClearPixels: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_ClearPixels(self, hnd);
        }

        case kFBCommand_BindSurface: {
            const int target = va_arg(ap, int);
            const int id = va_arg(ap, int);

            return GraphicsDriver_BindSurface(self, target, id);
        }


        case kFBCommand_CreateCLUT: {
            const size_t entryCount = va_arg(ap, size_t);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateCLUT(self, entryCount, hnd);
        }

        case kFBCommand_DestroyCLUT: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroyCLUT(self, hnd);
        }

        case kFBCommand_GetCLUTInfo: {
            int hnd = va_arg(ap, int);
            clut_info_t* ci = va_arg(ap, clut_info_t*);

            return GraphicsDriver_GetCLUTInfo(self, hnd, ci);
        }

        case kFBCommand_SetCLUTEntries: {
            const int hnd = va_arg(ap, int);
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return GraphicsDriver_SetCLUTEntries(self, hnd, idx, count, colors);
        }


        case kFBCommand_GetSpriteCaps: {
            sprite_caps_t* cp = va_arg(ap, sprite_caps_t*);

            GraphicsDriver_GetSpriteCaps(self, cp);
            return EOK;
        }

        case kFBCommand_SetSpritePosition: {
            const int hnd = va_arg(ap, int);
            const int x = va_arg(ap, int);
            const int y = va_arg(ap, int);

            return GraphicsDriver_SetSpritePosition(self, hnd, x, y);
        }

        case kFBCommand_SetSpriteVisible: {
            const int hnd = va_arg(ap, int);
            const bool flag = va_arg(ap, int);

            return GraphicsDriver_SetSpriteVisible(self, hnd, flag);
        }


        case kFBCommand_SetScreenConfig: {
            const intptr_t* cp = va_arg(ap, const intptr_t*);

            return GraphicsDriver_SetScreenConfig(self, cp);
        }

        case kFBCommand_GetScreenConfig: {
            intptr_t* cp = va_arg(ap, intptr_t*);
            size_t bufsiz = va_arg(ap, size_t);

            return GraphicsDriver_GetScreenConfig(self, cp, bufsiz);
        }

        case kFBCommand_SetScreenCLUTEntries: {
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const color_rgb32_t* colors = va_arg(ap, const color_rgb32_t*);

            return GraphicsDriver_SetScreenCLUTEntries(self, idx, count, colors);
        }

        default:
            return super_n(ioctl, Driver, GraphicsDriver, self, pChannel, cmd, ap);
    }
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
override_func_def(ioctl, GraphicsDriver, Driver)
override_func_def(getScreenSize, GraphicsDriver, DisplayDriver)
override_func_def(setScreenConfigObserver, GraphicsDriver, DisplayDriver)
override_func_def(setLightPenEnabled, GraphicsDriver, DisplayDriver)
override_func_def(obtainCursor, GraphicsDriver, DisplayDriver)
override_func_def(releaseCursor, GraphicsDriver, DisplayDriver)
override_func_def(bindCursor, GraphicsDriver, DisplayDriver)
override_func_def(setCursorPosition, GraphicsDriver, DisplayDriver)
override_func_def(setCursorVisible, GraphicsDriver, DisplayDriver)
);
