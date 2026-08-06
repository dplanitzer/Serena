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
// Mouse Cursor
//

errno_t IOHIDDisplay_setCursor(IOHIDDisplayRef _Nonnull self, const IOHIDCursor* _Nullable cursor)
{
    return EINVAL;
}

void IOHIDDisplay_updateCursor(IOHIDDisplayRef _Nonnull self, int16_t x, int16_t y, unsigned int flags)
{
}


class_func_defs(IOHIDDisplay, IODriver,
func_def(getScreenSize, IOHIDDisplay)
func_def(setScreenConfigObserver, IOHIDDisplay)
func_def(setCursor, IOHIDDisplay)
func_def(updateCursor, IOHIDDisplay)
);
