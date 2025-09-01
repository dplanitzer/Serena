//
//  GraphicsDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "GraphicsDriverPriv.h"
#include "copper.h"
#include "copper_comp.h"
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
    self->nextSurfaceId = 1;
    self->nextScreenId = 1;
    self->nextClutId = 1;
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


    // Allocate the Copper scheduler
    copper_init(self->nullSpriteData);

    
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


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Screens
////////////////////////////////////////////////////////////////////////////////

// Creates the even and odd field Copper programs for the given screen. There will
// always be at least an odd field program. The even field program will only exist
// for an interlaced screen.
static errno_t create_copper_prog(GraphicsDriverRef _Nonnull self, const VideoConfiguration* _Nonnull cfg, Surface* _Nonnull srf, ColorTable* _Nonnull clut, copper_prog_t _Nullable * _Nonnull pOutProg)
{
    decl_try_err();
    const bool isLightPenEnabled = self->flags.isLightPenEnabled;
    const size_t instrCount = copper_comp_calclength(srf, clut);
    copper_prog_t prog = NULL;
    copper_instr_t* ip;

    try(copper_prog_create(instrCount, &prog));
    prog->odd_entry = prog->prog;
    prog->even_entry = NULL;
    
    ip = prog->prog;
    ip = copper_comp_compile(ip, cfg, srf, clut, self->spriteDmaPtr, isLightPenEnabled, true);

    if (VideoConfiguration_IsInterlaced(cfg)) {
        prog->even_entry = ip;
        ip = copper_comp_compile(ip, cfg, srf, clut, self->spriteDmaPtr, isLightPenEnabled, false);
    }

catch:
    *pOutProg = prog;

    return err;
}

static int _get_config_value(const int* _Nonnull config, int key, int def)
{
    while (*config != SCREEN_CONFIG_END) {
        if (*config == key) {
            return *(config + 1);
        }
        config++;
    }

    return def;
}

static errno_t _create_fb(GraphicsDriverRef _Nonnull self, const int* _Nonnull config, Surface* _Nullable * _Nonnull pOutFb)
{
    int fb_id = _get_config_value(config, SCREEN_CONFIG_FRAMEBUFFER, 0);
    Surface* fb = _GraphicsDriver_GetSurfaceForId(self, fb_id);

    *pOutFb = fb;
    return EOK;
}

static errno_t _create_fb_clut(GraphicsDriverRef _Nonnull self, const int* _Nonnull config, ColorTable* _Nullable * _Nonnull pOutClut)
{
    int clut_id = _get_config_value(config, SCREEN_CONFIG_CLUT, 0);
    ColorTable* clut = _GraphicsDriver_GetClutForId(self, clut_id);

    *pOutClut = clut;
    return EOK;
}

// Sets the given screen as the current screen on the graphics driver. All graphics
// command apply to this new screen once this function has returned.
static errno_t GraphicsDriver_SetScreenConfig_Locked(GraphicsDriverRef _Nonnull _Locked self, const int* _Nullable config)
{
    decl_try_err();
    Surface* oldFb = self->fb;
    ColorTable* oldClut = self->clut;
    copper_prog_t prog = NULL;
    

    // Compile the Copper program(s) for the new screen
    if (config) {
        Surface* fb = NULL;
        ColorTable* clut = NULL;
        int fps = _get_config_value(config, SCREEN_CONFIG_FPS, 0);
        VideoConfiguration vc;

        err = _create_fb(self, config, &fb);
        if (err != EOK) {
            return err;
        }
        err = _create_fb_clut(self, config, &clut);
        if (err != EOK) {
            return err;
        }

        vc.width = Surface_GetWidth(fb);
        vc.height = Surface_GetHeight(fb);
        vc.fps = fps;

        err = create_copper_prog(self, &vc, fb, clut, &prog);
        if (err != EOK) {
            return err;
        }

        const bool isPal = VideoConfiguration_IsPAL(&vc);
        const bool isHires = VideoConfiguration_IsHires(&vc);
        const bool isLace = VideoConfiguration_IsInterlaced(&vc);

        self->hDiwStart = isPal ? DIW_PAL_HSTART : DIW_NTSC_HSTART;
        self->vDiwStart = isPal ? DIW_PAL_VSTART : DIW_NTSC_VSTART;
        self->hSprScale = isHires ? 0x01 : 0x00;
        self->vSprScale = isLace ? 0x01 : 0x00;

        self->vc = vc;
        self->fb = fb;
        self->clut = clut;
        Surface_BeginUse(fb);
        ColorTable_BeginUse(clut);
    }
    else {
        self->hDiwStart = 0;
        self->vDiwStart = 0;
        self->hSprScale = 0;
        self->vSprScale = 0;

        self->vc = (VideoConfiguration){0};
        self->fb = NULL;
        self->clut = NULL;
    } 


    // Schedule the new Copper program and wait until the new program is running
    // and the previous one has been retired. It's save to deallocate the old
    // framebuffer once the old program has stopped running.
    copper_schedule(prog, COPFLAG_WAIT_RUNNING);


    // Free the old screen
    if (oldClut) {
        ColorTable_EndUse(oldClut);
        if (!ColorTable_IsUsed(oldClut)) {
            ColorTable_Destroy(oldClut);
        }
    }
    if (oldFb) {
        Surface_EndUse(oldFb);
        if (!Surface_IsUsed(oldFb)) {
            Surface_Destroy(oldFb);
        }
    }

    return EOK;
}

errno_t GraphicsDriver_SetScreenConfig(GraphicsDriverRef _Nonnull self, const int* _Nullable config)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    err = GraphicsDriver_SetScreenConfig_Locked(self, config);
    mtx_unlock(&self->io_mtx);
    return err;
}

errno_t GraphicsDriver_GetScreenConfig(GraphicsDriverRef _Nonnull self, int* _Nonnull config, size_t bufsiz)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);
    if (bufsiz == 0) {
        throw(EINVAL);
    }

    //XXX implement me for real
    config[0] = SCREEN_CONFIG_END;

catch:
    mtx_unlock(&self->io_mtx);
    return err;
}

void GraphicsDriver_GetScreenSize(GraphicsDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    mtx_lock(&self->io_mtx);
    if (self->fb) {
        *pOutWidth = Surface_GetWidth(self->fb);
        *pOutHeight = Surface_GetHeight(self->fb);
    }
    else {
        *pOutWidth = 0;
        *pOutHeight = 0;
    }
    mtx_unlock(&self->io_mtx);
}

// Triggers an update of the display so that it accurately reflects the current
// display configuration.
errno_t GraphicsDriver_UpdateDisplay(GraphicsDriverRef _Nonnull self)
{
    decl_try_err();

    mtx_lock(&self->io_mtx);

    if (self->flags.isNewCopperProgNeeded) {
        copper_prog_t prog = NULL;

        err = create_copper_prog(self, &self->vc, self->fb, self->clut, &prog);
        if (err == EOK) {
            copper_schedule(prog, 0);
            self->flags.isNewCopperProgNeeded = 0;
        }
    }

    mtx_unlock(&self->io_mtx);
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Surfaces
////////////////////////////////////////////////////////////////////////////////

static int _GraphicsDriver_GetNewSurfaceId(GraphicsDriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextSurfaceId++;

        List_ForEach(&self->surfaces, Surface,
            if (Surface_GetId(pCurNode) == id) {
                hasCollision = true;
                break;
            }
        );
    }
    return id;
}

static Surface* _Nullable _GraphicsDriver_GetSurfaceForId(GraphicsDriverRef _Nonnull self, int id)
{
    List_ForEach(&self->surfaces, Surface,
        if (Surface_GetId(pCurNode) == id) {
            return pCurNode;
        }
    );
    return NULL;
}

errno_t GraphicsDriver_CreateSurface(GraphicsDriverRef _Nonnull self, int width, int height, PixelFormat pixelFormat, int* _Nonnull pOutId)
{
    Surface* srf;

    mtx_lock(&self->io_mtx);
    const errno_t err = Surface_Create(_GraphicsDriver_GetNewSurfaceId(self), width, height, pixelFormat, &srf);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->surfaces, &srf->chain);
        *pOutId = Surface_GetId(srf);
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
        if (!Surface_IsUsed(srf)) {
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

static int _GraphicsDriver_GetNewClutId(GraphicsDriverRef _Nonnull _Locked self)
{
    bool hasCollision = true;
    int id;

    while (hasCollision) {
        hasCollision = false;
        id = self->nextClutId++;

        List_ForEach(&self->colorTables, ColorTable,
            if (pCurNode->id == id) {
                hasCollision = true;
                break;
            }
        );
    }
    return id;
}

static ColorTable* _Nullable _GraphicsDriver_GetClutForId(GraphicsDriverRef _Nonnull self, int id)
{
    List_ForEach(&self->colorTables, ColorTable,
        if (pCurNode->id == id) {
            return pCurNode;
        }
    );
    return NULL;
}

errno_t GraphicsDriver_CreateCLUT(GraphicsDriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    ColorTable* clut;

    mtx_lock(&self->io_mtx);
    const errno_t err = ColorTable_Create(_GraphicsDriver_GetNewClutId(self), colorDepth, &clut);
    if (err == EOK) {
        List_InsertBeforeFirst(&self->colorTables, &clut->chain);
        *pOutId = clut->id;
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
        if (!ColorTable_IsUsed(clut)) {
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
        if (clut == self->clut) {
            self->flags.isNewCopperProgNeeded = 1;
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
