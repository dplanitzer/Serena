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
#include <kpi/hid.h>
#include <sched/vcpu.h>

typedef struct hid_cursor IOHIDCursor;

// UpdateCursor() flags
#define IOHID_CURSOR_CHANGE_POSITION    0x100   // Update cursor position
#define IOHID_CURSOR_CHANGE_VISIBILITY  0x200   // Update cursor visibility

#define IOHID_CURSOR_VISIBLE            0x1     // Cursor should be visible if set; invisible if not set


// A HID display is an abstraction over a video/graphics card which is used by
// the HID manager to manage the mouse cursor. It provides support for a
// hardware (preferable) or software mouse cursor.
//
// A HID display is an exclusive device.
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
    // Mouse Cursor
    //

    // Sets the pixel image of the mouse cursor.
    // Override: Required
    // Default: Does nothing and returns EINVAL
    errno_t (*setCursor)(void* _Nonnull self, const IOHIDCursor* _Nullable cursor);

    // Updates the visual state of the mouse cursor. The cursor is moved to the
    // position specified in 'state' and it is made visible or hidden based on
    // the flags in 'state'.
    // Override: Required
    // Default: Does nothing
    void (*updateCursor)(void* _Nonnull self, int16_t x, int16_t y, unsigned int flags);

    // Set a mouse cursor shielding rectangle. Mouse cursor shielding is turned
    // off if the area of teh specified rectangle is <= 0. Mouse cursor shielding
    // is only relevant for HID displays which implement the mouse cursor in
    // software. HID displays which implement the mouse cursor with the help of
    // (sprite) hardware should simply return immediately and ignore the shielding
    // rectangle parameters.
    // The purpose of the shielding rectangle is to ensure that the mouse cursor
    // is automatically hidden if it is inside the rectangle or it intersects
    // one of the rectangle edges or corners. If the mouse cursor intersects the
    // rectangle when this is function is called, then the mouse cursor should
    // be hidden right away.
    // The shielding rectangle coordinates are specified in terms of physical
    // display pixels.
    void (*shieldCursor)(void* _Nonnull self, int x, int y, int width, int height);
);


//
// Subclassers
//

#define IOHIDDisplay_GetScreenSize(__self, __w, __h) \
invoke_n(getScreenSize, IOHIDDisplay, __self, __w, __h)

#define IOHIDDisplay_SetScreenConfigObserver(__self, __vp, __signo) \
invoke_n(setScreenConfigObserver, IOHIDDisplay, __self, __vp, __signo)


#define IOHIDDisplay_SetCursor(__self, __cursor) \
invoke_n(setCursor, IOHIDDisplay, __self, __cursor)

#define IOHIDDisplay_UpdateCursor(__self, __x, __y, __flags) \
invoke_n(updateCursor, IOHIDDisplay, __self, __x, __y, __flags)

#define IOHIDDisplay_ShieldCursor(__self, __x, __y, __width, __height) \
invoke_n(shieldCursor, IOHIDDisplay, __self, __x, __y, __width, __height)

#endif /* IOHIDDisplay_h */
