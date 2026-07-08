//
//  IOHIDDisplay.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDDisplay_h
#define IOHIDDisplay_h

#include <driver/IODriver.h>
#include <kpi/framebuffer.h>
#include <sched/vcpu.h>


// A HID display is an abstraction over a video/graphics card which is used by
// the HID manager to manage the mouse cursor. It provides support for a
// hardware (preferable) or software mouse cursor.
//
open_class(IOHIDDisplay, IODriver,
);
open_class_funcs(IOHIDDisplay, IODriver,

    //
    // Screens
    //

    // Returns the width and height in terms of pixels of the currently active
    // screen configuration.
    // Override: Required
    // Default: Does nothing
    void (*getScreenSize)(void* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);

    // Specifies a VP that should receive the signal 'signo' every time the
    // current screen configuration changes. Turns notifications off if 'vp' is
    // NULL.
    // Override: Required
    // Default: Does nothing
    void (*setScreenConfigObserver)(void* _Nonnull self, vcpu_t _Nullable vp, int signo);


    //
    // Light Pens
    //

    // Enables or disables support for a light pen.
    // Override: Optional
    // Default: Does nothing
    void (*setLightPenEnabled)(void* _Nonnull self, bool enabled);


    //
    // Mouse Cursor
    //

    // Obtains the mouse cursor. The mouse cursor is initially transparent and thus
    // not visible on the screen. You assign an image to the mouse cursor by calling
    // the BindMouseCursor() function. Note that calling this function may forcefully
    // take ownership of the highest priority hardware sprites.
    // Override: Required
    // Default: Does nothing and returns ENOTSUP
    errno_t (*obtainCursor)(void* _Nonnull self);

    // Releases the mouse cursor and makes the underlying sprite available for
    // other uses again.
    // Override: Required
    // Default: Does nothing
    void (*releaseCursor)(void* _Nonnull self);

    // Sets the pixel image of the mouse cursor.
    // Override: Required
    // Default: Does nothing and returns ENOTSUP
    errno_t (*setCursor)(void* _Nonnull self, const void* _Nullable planes[], size_t bytesPerRow, int width, int height, pixfmt_t format);

    // Sets the position of the mouse cursor. Note that the mouse cursor is only
    // visible as long as at least some part of it is inside the display window
    // area.
    // Override: Required
    // Default: Does nothing
    void (*setCursorPosition)(void* _Nonnull self, int x, int y);

    // Sets the position of the mouse cursor. Note that the mouse cursor is only
    // visible as long as at least some part of it is inside the display window
    // area.
    // Override: Required
    // Default: Does nothing
    void (*setCursorVisible)(void* _Nonnull self, bool isVisible);
);


//
// Subclassers
//

#define IOHIDDisplay_GetScreenSize(__self, __w, __h) \
invoke_n(getScreenSize, IOHIDDisplay, __self, __w, __h)

#define IOHIDDisplay_SetScreenConfigObserver(__self, __vp, __signo) \
invoke_n(setScreenConfigObserver, IOHIDDisplay, __self, __vp, __signo)


#define IOHIDDisplay_SetLightPenEnabled(__self, __enabled) \
invoke_n(setLightPenEnabled, IOHIDDisplay, __self, __enabled)


#define IOHIDDisplay_ObtainCursor(__self) \
invoke_0(obtainCursor, IOHIDDisplay, __self)

#define IOHIDDisplay_ReleaseCursor(__self) \
invoke_0(releaseCursor, IOHIDDisplay, __self)

#define IOHIDDisplay_SetCursor(__self, __planes, __bytesPerRow, __width, __height, __format) \
invoke_n(setCursor, IOHIDDisplay, __self, __planes, __bytesPerRow, __width, __height, __format)

#define IOHIDDisplay_SetCursorPosition(__self, __x, __y) \
invoke_n(setCursorPosition, IOHIDDisplay, __self, __x, __y)

#define IOHIDDisplay_SetCursorVisible(__self, __vis) \
invoke_n(setCursorVisible, IOHIDDisplay, __self, __vis)

#endif /* IOHIDDisplay_h */
