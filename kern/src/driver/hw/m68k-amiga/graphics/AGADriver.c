//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include <string.h>
#include <handler/IOGraphicsHandler.h>

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

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AGADriver_start(AGADriverRef _Nonnull self)
{
    return gdInit();
}

bool AGADriver_isExclusive(AGADriverRef _Nonnull self)
{
    //XXX tmp as long as the console is still inside the kernel
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


//
// CLUT
//

errno_t AGADriver_CreateCLUT(AGADriverRef _Nonnull self, size_t colorDepth, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdGenClut(colorDepth, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroyCLUT(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDeleteClut(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetCLUTInfo(AGADriverRef _Nonnull self, int id, clut_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetClutInfo(id, pOutInfo);
    gdUnlock();
    return err;
}

// Sets the contents of 'count' consecutive CLUT entries starting at index 'idx'
// to the colors in the array 'entries'.
errno_t AGADriver_SetCLUTEntries(AGADriverRef _Nonnull self, int id, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdSetClutEntries(id, idx, count, entries);
    gdUnlock();
    return err;
}


//
// Pixel Buffer
//

errno_t AGADriver_CreateSurface2d(AGADriverRef _Nonnull self, int width, int height, pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdGenBuffer(width, height, pixelFormat, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_DestroySurface(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDeleteBuffer(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_BindSurface(AGADriverRef _Nonnull self, int target, int id)
{
    gdLock();
    const errno_t err = gdBindBuffer(target, id);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetSurfaceInfo(AGADriverRef _Nonnull self, int id, surface_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetBufferInfo(id, pOutInfo);
    gdUnlock();
    return err;
}

errno_t AGADriver_MapSurface(AGADriverRef _Nonnull self, int id, int mode, surface_mapping_t* _Nonnull pOutMapping)
{
    gdLock();
    const errno_t err = gdMapBuffer(id, mode, pOutMapping);
    gdUnlock();
    return err;
}

errno_t AGADriver_UnmapSurface(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdUnmapBuffer(id);
    gdUnlock();
    return err;
}

errno_t AGADriver_WritePixels(AGADriverRef _Nonnull self, int id, const void* _Nonnull planes[], size_t bytesPerRow, pixfmt_t format)
{
    gdLock();
    const errno_t err = gdWritePixels(id, planes, bytesPerRow, format);
    gdUnlock();
    return err;
}

errno_t AGADriver_ClearPixels(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdClearPixels(id);
    gdUnlock();
    return err;
}


//
// Sprites
//

errno_t AGADriver_SetSpritePosition(AGADriverRef _Nonnull self, int spriteId, int x, int y)
{
    gdLock();
    const errno_t err = gdSetSpritePos(spriteId, x, y);
    gdUnlock();
    return err;
}

errno_t AGADriver_SetSpriteVisible(AGADriverRef _Nonnull self, int spriteId, bool isVisible)
{
    gdLock();
    const errno_t err = gdSetSpriteVis(spriteId, isVisible);
    gdUnlock();
    return err;
}

void AGADriver_GetSpriteCaps(AGADriverRef _Nonnull self, sprite_caps_t* _Nonnull cp)
{
    gdLock();
    gdGetSpriteCaps(cp);
    gdUnlock();
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Mouse Cursor
////////////////////////////////////////////////////////////////////////////////

errno_t AGADriver_obtainCursor(AGADriverRef _Nonnull self)
{
    gdLock();
    const errno_t err = gdObtainCursor();
    gdUnlock();
    return err;
}

void AGADriver_releaseCursor(AGADriverRef _Nonnull self)
{
    gdLock();
    gdReleaseCursor();
    gdUnlock();
}

errno_t AGADriver_bindCursor(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdBindCursor(id);
    gdUnlock();
    return err;
}

void AGADriver_setCursorPosition(AGADriverRef _Nonnull self, int x, int y)
{
    gdLock();
    gdSetCursorPos(x, y);
    gdUnlock();
}

void AGADriver_setCursorVisible(AGADriverRef _Nonnull self, bool isVisible)
{
    gdLock();
    gdSetCursorVis(isVisible);
    gdUnlock();
}


//
// Screen
//

errno_t AGADriver_SetScreenConfig(AGADriverRef _Nonnull self, const intptr_t* _Nullable conf)
{
    gdLock();
    const errno_t err = gdSetScreenConfig(conf);
    gdUnlock();
    return err;
}

errno_t AGADriver_GetScreenConfig(AGADriverRef _Nonnull self, intptr_t* _Nonnull conf, size_t bufsiz)
{
    gdLock();
    const errno_t err = gdGetScreenConfig(conf, bufsiz);
    gdUnlock();
    return err;
}

errno_t AGADriver_SetScreenCLUTEntries(AGADriverRef _Nonnull self, size_t idx, size_t count, const color_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdSetScreenClutEntries(idx, count, entries);
    gdUnlock();
    return err;
}

void AGADriver_getScreenSize(AGADriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    gdGetScreenSize(pOutWidth, pOutHeight);
}

void AGADriver_setScreenConfigObserver(AGADriverRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    gdLock();
    gdSetScreenConfigObserver(vp, signo);
    gdUnlock();
}


void AGADriver_setLightPenEnabled(AGADriverRef _Nonnull self, bool enabled)
{
    gdLock();
    gdSetLightPenEnabled(enabled);
    gdUnlock();
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
