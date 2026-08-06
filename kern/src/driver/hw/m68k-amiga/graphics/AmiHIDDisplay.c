//
//  AmiHIDDisplay.c
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "AmiHIDDisplay.h"
#include "gd.h"
#include <kpi/hid.h>
#include <kpi/process.h>

#define MOUSE_SPRITE_PRI 0

IOCATS_DEF(g_cats, IOHID_DISPLAY);


errno_t AmiHIDDisplay_Create(AmiHIDDisplayRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiHIDDisplayRef self;
    
    try(IODriver_Create(class(AmiHIDDisplay), g_cats, (IODriverRef*)&self));
    self->cursorSpriteId = -1;

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AmiHIDDisplay_start(AmiHIDDisplayRef _Nonnull self)
{
    gdLock();
    const errno_t err = _gdCreateImage(PID_KERNELD, HID_CURSOR_WIDTH, HID_CURSOR_HEIGHT, GD_RGB_SPRITE_2, (struct image**)&self->cursorImage);
    gdUnlock();
    return err;
}

errno_t AmiHIDDisplay_setCursor(AmiHIDDisplayRef _Nonnull self, const IOHIDCursor* _Nullable cursor)
{
    decl_try_err();
    bool doRelease = false;

    if (cursor) {
        if (cursor->type != HID_STRUCTURE_TYPE_CURSOR) {
            return EINVAL;
        }
        if ((cursor->planes && cursor->bytesPerRow == 0) || cursor->width != HID_CURSOR_WIDTH || cursor->height != HID_CURSOR_HEIGHT || cursor->pixelFormat != HID_CURSOR_PIXELFORMAT) {
            return EINVAL;
        }
    }


    gdLock();
    if (cursor) {
        if (self->cursorSpriteId < 0) {
            // Acquire the mouse cursor sprite, since we previously didn't have it
            err = gdAcquireSprites(PID_KERNELD, MOUSE_SPRITE_PRI, 1, &self->cursorSpriteId);
            if (err == EOK) {
                err = _gdBindSpriteImage(PID_KERNELD, self->cursorSpriteId, self->cursorImage);
                if (err != EOK) {
                    doRelease = true;
                }
            }
        }


        if (err == EOK) {
            // Update the mouse cursor image
            _gdWritePixels(self->cursorImage, cursor->planes, cursor->bytesPerRow, cursor->pixelFormat);
        }
    }
    else if (self->cursorSpriteId >= 0) {
        // 'cursor' is NULL and we got a mouse cursor -> free the sprite
        // (but keep the cursor image memory around for future requests)
        doRelease = true;
    }


    if (doRelease) {
        gdReleaseSprites(PID_KERNELD, &self->cursorSpriteId, 1);
        self->cursorSpriteId = -1;
    }
    gdUnlock();
    return err;
}

void AmiHIDDisplay_updateCursor(AmiHIDDisplayRef _Nonnull self, int16_t x, int16_t y, unsigned int flags)
{
    gdLock();
    if ((flags & IOHID_CURSOR_CHANGE_POSITION) != 0) {
        gdMoveSprite(PID_KERNELD, self->cursorSpriteId, x, y);
    }
    if ((flags & IOHID_CURSOR_CHANGE_VISIBILITY) != 0) {
        gdShowSprite(PID_KERNELD, self->cursorSpriteId, (flags & IOHID_CURSOR_VISIBLE) ? true : false);
    }
    gdUnlock();
}

void AmiHIDDisplay_getScreenSize(AmiHIDDisplayRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    gd_display_mode_t mode;

    gdLock();
    gdGetDisplayInfo(GD_DISPLAY_MODE, &mode);
    gdUnlock();
    
    *pOutWidth = mode.width;
    *pOutHeight = mode.height;
}

void AmiHIDDisplay_setScreenConfigObserver(AmiHIDDisplayRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
    gdLock();
    gdSetScreenConfigObserver(vp, signo);
    gdUnlock();
}


class_func_defs(AmiHIDDisplay, IOHIDDisplay,
override_func_def(start, AmiHIDDisplay, IODriver)
override_func_def(getScreenSize, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setScreenConfigObserver, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setCursor, AmiHIDDisplay, IOHIDDisplay)
override_func_def(updateCursor, AmiHIDDisplay, IOHIDDisplay)
);
