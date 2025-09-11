//
//  DisplayDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DisplayDriver_h
#define DisplayDriver_h

#include <driver/Driver.h>
#include <kpi/fb.h>
#include <sched/vcpu.h>


// A display driver is responsible for managing framebuffers,  offscreen pixel
// buffers, CLUTs, sprites and the mouse cursor.
//
// A display driver may be dumb - meaning that it does not support any form of
// hardware accelerated pixel processing nor any sprites. A display driver like
// this is however still required to implement support for a mouse cursor. It
// must implement this support in software.
//
// A display driver may be smart - meaning that it is able to offload certain or
// all pixel processing operations to dedicated hardware. It may also support
// one or more hardware sprites. A driver like this should implement the mouse
// cursor support by using the highest priority hardware sprite available.
//
open_class(DisplayDriver, Driver,
);
open_class_funcs(DisplayDriver, Driver,

    //
    // Screens
    //

    // Returns the width and height in terms of pixels of the currently active
    // screen configuration.
    // Override: Required
    // Default Behavior: Does nothing
    void (*getScreenSize)(void* _Nonnull self, int* _Nonnull pOutWidth, int* _Nonnull pOutHeight);

    // Specifies a VP that should receive the signal 'signo' every time the
    // current screen configuration changes. Turns notifications off if 'vp' is
    // NULL.
    // Override: Required
    // Default Behavior: Does nothing
    void (*setScreenConfigObserver)(void* _Nonnull self, vcpu_t _Nullable vp, int signo);


    //
    // Light Pens
    //

    // Enables or disables support for a light pen.
    // Override: Optional
    // Default Behavior: Does nothing
    void (*setLightPenEnabled)(void* _Nonnull self, bool enabled);


    //
    // Mouse Cursor
    //

    // Obtains the mouse cursor. The mouse cursor is initially transparent and thus
    // not visible on the screen. You assign an image to the mouse cursor by calling
    // the BindMouseCursor() function. Note that calling this function may forcefully
    // take ownership of the highest priority hardware sprites.
    // Override: Required
    // Default Behavior: Does nothing and returns ENOTSUP
    errno_t (*obtainCursor)(void* _Nonnull self);

    // Releases the mouse cursor and makes the underlying sprite available for
    // other uses again.
    // Override: Required
    // Default Behavior: Does nothing
    void (*releaseCursor)(void* _Nonnull self);

    // Binds the given surface to the mouse cursor.
    // Override: Required
    // Default Behavior: Does nothing and returns ENOTSUP
    errno_t (*bindCursor)(void* _Nonnull self, int id);

    // Sets the position of the mouse cursor. Note that the mouse cursor is only
    // visible as long as at least some part of it is inside the display window
    // area.
    // Override: Required
    // Default Behavior: Does nothing
    void (*setCursorPosition)(void* _Nonnull self, int x, int y);

    // Sets the position of the mouse cursor. Note that the mouse cursor is only
    // visible as long as at least some part of it is inside the display window
    // area.
    // Override: Required
    // Default Behavior: Does nothing
    void (*setCursorVisible)(void* _Nonnull self, bool isVisible);
);


//
// Subclassers
//

#define DisplayDriver_GetScreenSize(__self, __w, __h) \
invoke_n(getScreenSize, DisplayDriver, __self, __w, __h)

#define DisplayDriver_SetScreenConfigObserver(__self, __vp, __signo) \
invoke_n(setScreenConfigObserver, DisplayDriver, __self, __vp, __signo)


#define DisplayDriver_SetLightPenEnabled(__self, __enabled) \
invoke_n(setLightPenEnabled, DisplayDriver, __self, __enabled)


#define DisplayDriver_ObtainCursor(__self) \
invoke_0(obtainCursor, DisplayDriver, __self)

#define DisplayDriver_ReleaseCursor(__self) \
invoke_0(releaseCursor, DisplayDriver, __self)

#define DisplayDriver_BindCursor(__self, __id) \
invoke_n(bindCursor, DisplayDriver, __self, __id)

#define DisplayDriver_SetCursorPosition(__self, __x, __y) \
invoke_n(setCursorPosition, DisplayDriver, __self, __x, __y)

#define DisplayDriver_SetCursorVisible(__self, __vis) \
invoke_n(setCursorVisible, DisplayDriver, __self, __vis)

#endif /* DisplayDriver_h */
