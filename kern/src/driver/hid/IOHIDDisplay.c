//
//  IOHIDDisplay.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "IOHIDDisplay.h"

//
// Screens
//

void IOHIDDisplay_getScreenSize(IOHIDDisplayRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    *pOutWidth = 0;
    *pOutHeight = 0;
}

void IOHIDDisplay_setScreenConfigObserver(IOHIDDisplayRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
}


//
// Light Pens
//

void IOHIDDisplay_setLightPenEnabled(IOHIDDisplayRef _Nonnull self, bool enabled)
{
}


//
// Mouse Cursor
//

errno_t IOHIDDisplay_obtainCursor(IOHIDDisplayRef _Nonnull self)
{
    return ENOTSUP;
}

void IOHIDDisplay_releaseCursor(IOHIDDisplayRef _Nonnull self)
{
}

errno_t IOHIDDisplay_setCursor(IOHIDDisplayRef _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, pixfmt_t format)
{
    return ENOTSUP;
}

void IOHIDDisplay_setCursorPosition(IOHIDDisplayRef _Nonnull self, int x, int y)
{
}

void IOHIDDisplay_setCursorVisible(IOHIDDisplayRef _Nonnull self, bool isVisible)
{
}


class_func_defs(IOHIDDisplay, IODriver,
func_def(getScreenSize, IOHIDDisplay)
func_def(setScreenConfigObserver, IOHIDDisplay)
func_def(setLightPenEnabled, IOHIDDisplay)
func_def(obtainCursor, IOHIDDisplay)
func_def(releaseCursor, IOHIDDisplay)
func_def(setCursor, IOHIDDisplay)
func_def(setCursorPosition, IOHIDDisplay)
func_def(setCursorVisible, IOHIDDisplay)
);
