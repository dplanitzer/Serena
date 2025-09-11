//
//  DisplayDriver.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "DisplayDriver.h"

//
// Screens
//

void DisplayDriver_getScreenSize(DisplayDriverRef _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight)
{
    *pOutWidth = 0;
    *pOutHeight = 0;
}

void DisplayDriver_setScreenConfigObserver(DisplayDriverRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
}


//
// Light Pens
//

void DisplayDriver_setLightPenEnabled(DisplayDriverRef _Nonnull self, bool enabled)
{
}


//
// Mouse Cursor
//

errno_t DisplayDriver_obtainCursor(DisplayDriverRef _Nonnull self)
{
    return ENOTSUP;
}

void DisplayDriver_releaseCursor(DisplayDriverRef _Nonnull self)
{
}

errno_t DisplayDriver_bindCursor(DisplayDriverRef _Nonnull self, int id)
{
    return ENOTSUP;
}

void DisplayDriver_setCursorPosition(DisplayDriverRef _Nonnull self, int x, int y)
{
}

void DisplayDriver_setCursorVisible(DisplayDriverRef _Nonnull self, bool isVisible)
{
}


class_func_defs(DisplayDriver, Driver,
func_def(getScreenSize, DisplayDriver)
func_def(setScreenConfigObserver, DisplayDriver)
func_def(setLightPenEnabled, DisplayDriver)
func_def(obtainCursor, DisplayDriver)
func_def(releaseCursor, DisplayDriver)
func_def(bindCursor, DisplayDriver)
func_def(setCursorPosition, DisplayDriver)
func_def(setCursorVisible, DisplayDriver)
);
