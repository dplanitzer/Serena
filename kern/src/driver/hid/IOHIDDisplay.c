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

void IOHIDDisplay_getScreenResolution(IOHIDDisplayRef _Nonnull self, int16_t* _Nonnull pOutWidth, int16_t* _Nonnull pOutHeight)
{
    *pOutWidth = 0;
    *pOutHeight = 0;
}

void IOHIDDisplay_setChangeSignal(IOHIDDisplayRef _Nonnull self, vcpu_t _Nullable vp, int signo)
{
}


//
// Mouse Cursor
//

errno_t IOHIDDisplay_setCursor(IOHIDDisplayRef _Nonnull self, const IOHIDCursor* _Nullable cursor)
{
    return EINVAL;
}

void IOHIDDisplay_updateCursor(IOHIDDisplayRef _Nonnull self, int16_t x, int16_t y, unsigned int flags)
{
}

void IOHIDDisplay_shieldCursor(IOHIDDisplayRef _Nonnull self, int x, int y, int width, int height)
{
}


class_func_defs(IOHIDDisplay, IODriver,
func_def(getScreenResolution, IOHIDDisplay)
func_def(setChangeSignal, IOHIDDisplay)
func_def(setCursor, IOHIDDisplay)
func_def(updateCursor, IOHIDDisplay)
func_def(shieldCursor, IOHIDDisplay)
);
