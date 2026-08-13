//
//  AGADriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "AGADriverPriv.h"
#include "video_conf.h"
#include <ext/math.h>
#include <string.h>
#include <handler/IOGraphicsHandler.h>
#include <process/Process.h>

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

void AGADriver_doClose(AGADriverRef _Nonnull _Locked self)
{
//    const pid_t pid = Process_GetCurrentId();

    IODriver_Super_DoClose(AGADriver);

    gdLock();
    //XXX temp disabled until the console has been moved to user space
//    gdReleaseSprites(pid, NULL, 0);
//    gdDestroyImagesOwnedBy(pid);
//    gdDestroyCommandBuffersOwnedBy(pid);
    gdUnlock();
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
// Images
//

errno_t AGADriver_createImage(AGADriverRef _Nonnull self, int width, int height, gd_pixfmt_t pixelFormat, int* _Nonnull pOutId)
{
    gdLock();
    const errno_t err = gdCreateImage(Process_GetCurrentId(), width, height, pixelFormat, pOutId);
    gdUnlock();
    return err;
}

errno_t AGADriver_destroyImage(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDestroyImage(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}

errno_t AGADriver_getImageInfo(AGADriverRef _Nonnull self, int id, gd_image_info_t* _Nonnull pOutInfo)
{
    gdLock();
    const errno_t err = gdGetImageInfo(Process_GetCurrentId(), id, pOutInfo);
    gdUnlock();
    return err;
}

errno_t AGADriver_mapImage(AGADriverRef _Nonnull self, int id, int mode, gd_image_data_t* _Nonnull pOutMapping)
{
    gdLock();
    const errno_t err = gdMapImage(Process_GetCurrentId(), id, mode, pOutMapping);
    gdUnlock();
    return err;
}

errno_t AGADriver_unmapImage(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdUnmapImage(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}


//
// Sprites
//

errno_t AGADriver_acquireSprites(AGADriverRef _Nonnull self, int basePriority, size_t count, int* _Nonnull pOutSpriteIds)
{
    gdLock();
    const errno_t err = gdAcquireSprites(Process_GetCurrentId(), basePriority, count, pOutSpriteIds);
    gdUnlock();
    return err;
}

errno_t AGADriver_releaseSprites(AGADriverRef _Nonnull self, const int* _Nullable spriteIds, size_t count)
{
    gdLock();
    const errno_t err = gdReleaseSprites(Process_GetCurrentId(), spriteIds, count);
    gdUnlock();
    return err;
}

void AGADriver_getSpriteConstraints(AGADriverRef _Nonnull self, gd_sprite_constraints_t* _Nonnull cp)
{
    gdLock();
    gdGetSpriteConstraints(cp);
    gdUnlock();
}


//
// CLUT
//

errno_t AGADriver_clut(AGADriverRef _Nonnull self, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdClut(idx, count, entries);
    gdUnlock();
    return err;
}

errno_t AGADriver_getClut(AGADriverRef _Nonnull self, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
{
    gdLock();
    const errno_t err = gdGetClut(idx, count, entries);
    gdUnlock();
    return err;
}

errno_t AGADriver_getClutInfo(AGADriverRef _Nonnull self, gd_clut_info_t* _Nonnull info)
{
    gdLock();
    const errno_t err = gdGetClutInfo(info);
    gdUnlock();
    return err;
}


//
// Display
//

errno_t AGADriver_displayMode(AGADriverRef _Nonnull self, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op)
{
    gdLock();
    const errno_t err = gdDisplayMode(mode, params, op);
    gdUnlock();
    return err;
}

errno_t AGADriver_getDisplayInfo(AGADriverRef _Nonnull self, int flavor, gd_display_info_ref_t _Nonnull info)
{
    gdLock();
    const errno_t err = gdGetDisplayInfo(flavor, info);
    gdUnlock();
    return err;
}

errno_t AGADriver_enumDisplayModes(AGADriverRef _Nonnull self, int index, gd_display_mode_t* _Nonnull pOutMode)
{
    gdLock();
    const errno_t err = gdEnumDisplayModes(index, pOutMode);
    gdUnlock();
    return err;
}


//
// Command Buffers
//

errno_t AGADriver_createCommandBuffer(AGADriverRef _Nonnull self, size_t size, gd_cmdbuf_desc_t* _Nonnull desc)
{
    gdLock();
    const errno_t err = gdCreateCommandBuffer(Process_GetCurrentId(), size, desc);
    gdUnlock();
    return err;
}

errno_t AGADriver_destroyCommandBuffer(AGADriverRef _Nonnull self, int id)
{
    gdLock();
    const errno_t err = gdDestroyCommandBuffer(Process_GetCurrentId(), id);
    gdUnlock();
    return err;
}

errno_t AGADriver_submitCommandBuffer(AGADriverRef _Nonnull self, int queue_id, int cmds_id)
{
    gdLock();
    const errno_t err = gdSubmitCommandBuffer(Process_GetCurrentId(), queue_id, cmds_id);
    gdUnlock();
    return err;
}


class_func_defs(AGADriver, IOGraphicsDriver,
override_func_def(start, AGADriver, IODriver)
override_func_def(doClose, AGADriver, IODriver)
override_func_def(getDFSInfo, AGADriver, IODriver)
override_func_def(createImage, AGADriver, IOGraphicsDriver)
override_func_def(destroyImage, AGADriver, IOGraphicsDriver)
override_func_def(getImageInfo, AGADriver, IOGraphicsDriver)
override_func_def(mapImage, AGADriver, IOGraphicsDriver)
override_func_def(unmapImage, AGADriver, IOGraphicsDriver)
override_func_def(acquireSprites, AGADriver, IOGraphicsDriver)
override_func_def(releaseSprites, AGADriver, IOGraphicsDriver)
override_func_def(getSpriteConstraints, AGADriver, IOGraphicsDriver)
override_func_def(createCommandBuffer, AGADriver, IOGraphicsDriver)
override_func_def(destroyCommandBuffer, AGADriver, IOGraphicsDriver)
override_func_def(submitCommandBuffer, AGADriver, IOGraphicsDriver)
override_func_def(clut, AGADriver, IOGraphicsDriver)
override_func_def(getClut, AGADriver, IOGraphicsDriver)
override_func_def(getClutInfo, AGADriver, IOGraphicsDriver)
override_func_def(displayMode, AGADriver, IOGraphicsDriver)
override_func_def(getDisplayInfo, AGADriver, IOGraphicsDriver)
override_func_def(enumDisplayModes, AGADriver, IOGraphicsDriver)
);
