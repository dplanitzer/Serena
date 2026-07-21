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

IOCATS_DEF(g_cats, IOHID_DISPLAY);


errno_t AmiHIDDisplay_Create(AmiHIDDisplayRef _Nullable * _Nonnull pOutSelf)
{
    decl_try_err();
    AmiHIDDisplayRef self;
    
    try(IODriver_Create(class(AmiHIDDisplay), g_cats, (IODriverRef*)&self));

    *pOutSelf = self;
    return EOK;

catch:
    Object_Release(self);
    *pOutSelf = NULL;
    return err;
}

errno_t AmiHIDDisplay_start(AmiHIDDisplayRef _Nonnull self)
{
    return EOK;
}

errno_t AmiHIDDisplay_obtainCursor(AmiHIDDisplayRef _Nonnull self)
{
    gdLock();
    const errno_t err = gdObtainCursor();
    gdUnlock();
    return err;
}

void AmiHIDDisplay_releaseCursor(AmiHIDDisplayRef _Nonnull self)
{
    gdLock();
    gdReleaseCursor();
    gdDeleteBuffer(self->cursorBufferId);
    self->cursorBufferId = 0;
    gdUnlock();
}

errno_t AmiHIDDisplay_setCursor(AmiHIDDisplayRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, gd_pixfmt_t format)
{
    decl_try_err();

    if ((planes && bytesPerRow == 0) || width != HID_CURSOR_WIDTH || height != HID_CURSOR_HEIGHT || format != HID_CURSOR_PIXELFORMAT) {
        return EINVAL;
    }

    
    gdLock();
    if (self->cursorBufferId == 0 || self->cursorWidth != width || self->cursorHeight != height) {
        int newId;

        try(gdGenBuffer(width, height, GD_RGB_SPRITE_2, &newId));
        self->cursorWidth = width;
        self->cursorHeight = height;

        if (self->cursorBufferId) {
            gdDeleteBuffer(self->cursorBufferId);
        }
        self->cursorBufferId = newId;
    }

    try(_gdDrawPixels(self->cursorBufferId, planes, bytesPerRow, format));
    try(gdBindCursor(self->cursorBufferId));

catch:
    gdUnlock();
    return err;
}

void AmiHIDDisplay_setCursorPosition(AmiHIDDisplayRef _Nonnull self, int x, int y)
{
    gdLock();
    gdSetCursorPos(x, y);
    gdUnlock();
}

void AmiHIDDisplay_setCursorVisible(AmiHIDDisplayRef _Nonnull self, bool isVisible)
{
    gdLock();
    gdSetCursorVis(isVisible);
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
override_func_def(obtainCursor, AmiHIDDisplay, IOHIDDisplay)
override_func_def(releaseCursor, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setCursor, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setCursorPosition, AmiHIDDisplay, IOHIDDisplay)
override_func_def(setCursorVisible, AmiHIDDisplay, IOHIDDisplay)
);
