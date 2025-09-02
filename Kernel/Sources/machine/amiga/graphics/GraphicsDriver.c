//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include <kern/kalloc.h>
#include <kern/timespec.h>
#include <kpi/fcntl.h>
#include <kpi/hid.h>
#include <machine/irq.h>

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


    // Allocate the null and mouse cursor sprite
    Sprite_Init(&self->mouseCursor);
    try(Sprite_Acquire(&self->mouseCursor, kMouseCursor_Width, kMouseCursor_Height, kMouseCursor_PixelFormat));
    
    try(kalloc_options(sizeof(uint16_t) * 6, KALLOC_OPTION_UNIFIED, (void**)&self->nullSpriteData));
    self->nullSpriteData[0] = 0x1905;
    self->nullSpriteData[1] = 0x1a00;
    self->nullSpriteData[2] = 0;
    self->nullSpriteData[3] = 0;
    self->nullSpriteData[4] = 0;
    self->nullSpriteData[5] = 0;

    for (int i = 0; i < SPRITE_COUNT; i++) {
        Sprite_Init(&self->sprite[i]);
        self->spriteDmaPtr[i] = self->nullSpriteData;
    }


    // Create a null Copper program
    copper_prog_t nullCopperProg;
    try(GraphicsDriver_CreateNullCopperProg(self, &nullCopperProg));


    // Allocate the Copper management VCPU
    wq_init(&self->copvpWaitQueue);
    self->copvpSigs = _SIGBIT(SIGCOPRET);

    _vcpu_acquire_attr_t attr;
    attr.func = (vcpu_func_t)GraphicsDriver_CopperManager;
    attr.arg = self;
    attr.stack_size = 0;
    attr.groupid = VCPUID_MAIN_GROUP;
    attr.sched_params.qos = VCPU_QOS_INTERACTIVE;
    attr.sched_params.priority = VCPU_PRI_NORMAL - 1;
    attr.flags = 0;
    attr.data = 0;
    try(Process_AcquireVirtualProcessor(gKernelProcess, &attr, &self->copvp));

    
    // Initialize the Copper scheduler
    copper_init(nullCopperProg, SIGCOPRET, self->copvp);

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
    de.uid = kUserId_Root;
    de.gid = kGroupId_Root;
    de.perms = perm_from_octal(0666);
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
        case kFBCommand_CreateSurface: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const PixelFormat fmt = va_arg(ap, PixelFormat);
            int* hnd = va_arg(ap, int*);

            return GraphicsDriver_CreateSurface(self, width, height, fmt, hnd);
        }

        case kFBCommand_DestroySurface: {
            int hnd = va_arg(ap, int);

            return GraphicsDriver_DestroySurface(self, hnd);
        }

        case kFBCommand_GetSurfaceInfo: {
            int hnd = va_arg(ap, int);
            SurfaceInfo* si = va_arg(ap, SurfaceInfo*);

            return GraphicsDriver_GetSurfaceInfo(self, hnd, si);
        }

        case kFBCommand_MapSurface: {
            int hnd = va_arg(ap, int);
            MapPixels mode = va_arg(ap, MapPixels);
            SurfaceMapping* sm = va_arg(ap, SurfaceMapping*);

            return GraphicsDriver_MapSurface(self, hnd, mode, sm);
        }

        case kFBCommand_UnmapSurface: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_UnmapSurface(self, hnd);
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
            CLUTInfo* ci = va_arg(ap, CLUTInfo*);

            return GraphicsDriver_GetCLUTInfo(self, hnd, ci);
        }

        case kFBCommand_SetCLUTEntries: {
            const int hnd = va_arg(ap, int);
            const size_t idx = va_arg(ap, size_t);
            const size_t count = va_arg(ap, size_t);
            const RGBColor32* colors = va_arg(ap, const RGBColor32*);

            return GraphicsDriver_SetCLUTEntries(self, hnd, idx, count, colors);
        }


        case kFBCommand_AcquireSprite: {
            const int width = va_arg(ap, int);
            const int height = va_arg(ap, int);
            const PixelFormat fmt = va_arg(ap, PixelFormat);
            const int pri = va_arg(ap, int);
            int* sid = va_arg(ap, int*);

            return GraphicsDriver_AcquireSprite(self, width, height, fmt, pri, sid);
        }

        case kFBCommand_RelinquishSprite: {
            const int hnd = va_arg(ap, int);

            return GraphicsDriver_RelinquishSprite(self, hnd);
        }

        case kFBCommand_SetSpritePixels: {
            const int hnd = va_arg(ap, int);
            const uint16_t** planes = va_arg(ap, const uint16_t**);

            return GraphicsDriver_SetSpritePixels(self, hnd, planes);
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
            const int* cp = va_arg(ap, const int*);

            return GraphicsDriver_SetScreenConfig(self, cp);
        }

        case kFBCommand_GetScreenConfig: {
            int* cp = va_arg(ap, int*);
            size_t bufsiz = va_arg(ap, size_t);

            return GraphicsDriver_GetScreenConfig(self, cp, bufsiz);
        }

        case kFBCommand_UpdateDisplay:
            return GraphicsDriver_UpdateDisplay(self);


        default:
            return super_n(ioctl, Driver, GraphicsDriver, self, pChannel, cmd, ap);
    }
}


static int _GraphicsDriver_GetNewGObjId(GraphicsDriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextGObjId++;

        List_ForEach(&self->gobjs, GObject,
            if (GObject_GetId(pCurNode) == id) {
                hasCollision = true;
                break;
            }
        );
    }
    return id;
}

void* _Nullable _GraphicsDriver_GetGObjForId(GraphicsDriverRef _Nonnull self, int id, int type)
{
    List_ForEach(&self->gobjs, GObject,
        if (GObject_GetId(pCurNode) == id) {
            return (GObject_GetType(pCurNode) == type) ? pCurNode : NULL;
        }
    );
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Surfaces
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
{
    Surface* srf;

    mtx_lock(&self->io_mtx);
    const errno_t err = Surface_Create(_GraphicsDriver_GetNewGObjId(self), width, height, pixelFormat, &srf);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->gobjs, GObject_GetChainPtr(srf));
        *pOutId = GObject_GetId(srf);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_DestroySurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        if (!GObject_IsUsed(srf)) {
            Surface_Destroy(srf);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetSurfaceInfo(GraphicsDriverRef _Nonnull self, int id, SurfaceInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);

    if (srf) {
        pOutInfo->width = Surface_GetWidth(srf);
        pOutInfo->height = Surface_GetHeight(srf);
        pOutInfo->pixelFormat = Surface_GetPixelFormat(srf);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

errno_t GraphicsDriver_MapSurface(GraphicsDriverRef _Nonnull self, int id, MapPixels mode, SurfaceMapping* _Nonnull pOutMapping)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Map(srf, mode, pOutMapping);
    }
    else {
        err = ENOTSUP;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_UnmapSurface(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    Surface* srf = _GraphicsDriver_GetSurfaceForId(self, id);
    if (srf) {
        err = Surface_Unmap(srf);
    }
    else {
        err = ENOTSUP;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: CLUT
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    ColorTable* clut;

    mtx_lock(&self->io_mtx);
    const errno_t err = ColorTable_Create(_GraphicsDriver_GetNewGObjId(self), colorDepth, &clut);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->gobjs, GObject_GetChainPtr(clut));
        *pOutId = GObject_GetId(clut);
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_DestroyCLUT(GraphicsDriverRef _Nonnull self, int id)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);

    if (clut) {
        if (!GObject_IsUsed(clut)) {
            ColorTable_Destroy(clut);
        }
        else {
            err = EBUSY;
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetCLUTInfo(GraphicsDriverRef _Nonnull self, int id, CLUTInfo* _Nonnull pOutInfo)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);

    if (clut) {
        pOutInfo->entryCount = clut->entryCount;
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t GraphicsDriver_SetCLUTEntries(GraphicsDriverRef _Nonnull self, int id, size_t idx, size_t count, const RGBColor32* _Nonnull entries)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, id);
    if (clut) {
        err = ColorTable_SetEntries(clut, idx, count, entries);
        if (err == EOK) {
            const unsigned sim = irq_set_mask(IRQ_MASK_VBLANK);

            if (clut == (ColorTable*)g_copper_running_prog->res.clut) {
                self->flags.isNewCopperProgNeeded = 1;
            }
            irq_set_mask(sim);
        }
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Sprites
////////////////////////////////////////////////////////////////////////////////

#define MAKE_SPRITE_ID(__sprIdx) \
((__sprIdx) + 1)

#define GET_SPRITE_IDX(__sprId) \
((__sprId) - 1)


// Acquires a hardware sprite
errno_t GraphicsDriver_AcquireSprite(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int priority, int* _Nonnull pOutSpriteId)
{
    decl_try_err();

    if (priority < 0 || priority >= SPRITE_COUNT) {
        return ENOTSUP;
    }

    mtx_lock(&self->io_mtx);
    if (self->spriteDmaPtr[priority] != self->nullSpriteData) {
        throw(EBUSY);
    }

    try(Sprite_Acquire(&self->sprite[priority], width, height, pixelFormat));
    self->spriteDmaPtr[priority] = self->sprite[priority].data;
    self->flags.isNewCopperProgNeeded = 1;

catch:
    mtx_unlock(&self->io_mtx);
    *pOutSpriteId = (err == EOK) ? MAKE_SPRITE_ID(priority) : 0;
    return err;
}

// Relinquishes a hardware sprite
errno_t GraphicsDriver_RelinquishSprite(GraphicsDriverRef _Nonnull self, int spriteId)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    if (sprIdx < 0) {
        return EOK;
    }
    if (sprIdx >= SPRITE_COUNT) {
        return EINVAL;
    }

    mtx_lock(&self->io_mtx);
    if (sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        // XXX Sprite_Destroy(pScreen->sprite[spriteId]);
        // XXX actually free the old sprite instead of leaking it. Can't do this
        // XXX yet because we need to ensure that the DMA is no longer accessing
        // XXX the data before it freeing it.
        self->spriteDmaPtr[sprIdx] = self->nullSpriteData;
        self->flags.isNewCopperProgNeeded = 1;
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_SetSpritePixels(GraphicsDriverRef _Nonnull self, int spriteId, const uint16_t* _Nonnull planes[2])
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        Sprite_SetPixels(&self->sprite[sprIdx], planes);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

// Updates the position of a hardware sprite.
errno_t GraphicsDriver_SetSpritePosition(GraphicsDriverRef _Nonnull self, int spriteId, int x, int y)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        const int16_t x16 = __max(__min(x, INT16_MAX), INT16_MIN);
        const int16_t y16 = __max(__min(y, INT16_MAX), INT16_MIN);
        const int16_t sprX = self->hDiwStart - 1 + (x16 >> self->hSprScale);
        const int16_t sprY = self->vDiwStart + (y16 >> self->vSprScale);

        Sprite_SetPosition(&self->sprite[sprIdx], sprX, sprY);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}

// Updates the visibility of a hardware sprite.
errno_t GraphicsDriver_SetSpriteVisible(GraphicsDriverRef _Nonnull self, int spriteId, bool isVisible)
{
    decl_try_err();
    const int sprIdx = GET_SPRITE_IDX(spriteId);

    mtx_lock(&self->io_mtx);
    if (sprIdx >= 0 && sprIdx < SPRITE_COUNT && self->sprite[sprIdx].isAcquired) {
        Sprite_SetVisible(&self->sprite[sprIdx], isVisible);
    }
    else {
        err = EINVAL;
    }
    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Light Pen
////////////////////////////////////////////////////////////////////////////////

// Enables / disables the h/v raster position latching triggered by a light pen.
void GraphicsDriver_SetLightPenEnabled(GraphicsDriverRef _Nonnull self, bool enabled)
{
    mtx_lock(&self->io_mtx);
    if (self->flags.isLightPenEnabled != enabled) {
        self->flags.isLightPenEnabled = enabled;
        self->flags.isNewCopperProgNeeded = 1;
    }
    mtx_unlock(&self->io_mtx);
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t GraphicsDriver_SetMouseCursor(GraphicsDriverRef _Nonnull self, const uint16_t* _Nullable planes[2], int width, int height, PixelFormat pixelFormat)
{
    if ((width > 0 && height > 0) && (width != kMouseCursor_Width || height != kMouseCursor_Height || pixelFormat != kMouseCursor_PixelFormat)) {
        return ENOTSUP;
    }

    mtx_lock(&self->io_mtx);
    const unsigned oldMouseCursorEnabled = self->flags.mouseCursorEnabled;

    if (width > 0 && height > 0) {
        Sprite_SetPixels(&self->mouseCursor, planes);
        self->flags.mouseCursorEnabled = 1;
    }
    else {
        self->flags.mouseCursorEnabled = 0;
    }

    if (oldMouseCursorEnabled != self->flags.mouseCursorEnabled) {
        self->spriteDmaPtr[MOUSE_SPRITE_PRI] = (self->flags.mouseCursorEnabled) ? self->mouseCursor.data : self->nullSpriteData;
        self->flags.isNewCopperProgNeeded = 1;
    }
    mtx_unlock(&self->io_mtx);
    return EOK;
}

void GraphicsDriver_SetMouseCursorPosition(GraphicsDriverRef _Nonnull self, int x, int y)
{
    mtx_lock(&self->io_mtx);
    const int16_t x16 = __max(__min(x, INT16_MAX), INT16_MIN);
    const int16_t y16 = __max(__min(y, INT16_MAX), INT16_MIN);
    const int16_t sprX = self->hDiwStart - 1 + (x16 >> self->hSprScale);
    const int16_t sprY = self->vDiwStart + (y16 >> self->vSprScale);

    Sprite_SetPosition(&self->mouseCursor, sprX, sprY);
    mtx_unlock(&self->io_mtx);
}


class_func_defs(GraphicsDriver, Driver,
override_func_def(onStart, GraphicsDriver, Driver)
override_func_def(ioctl, GraphicsDriver, Driver)
);
