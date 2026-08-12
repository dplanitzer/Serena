//
//  IOHIDDisplay.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDDisplay_h
#define IOHIDDisplay_h

#include <stdbool.h>
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
// A HID display which implements the mouse cursor in software must implement
// support for mouse cursor shielding. NO shield is active by default. It is
// made active when a call to teh shieldCursor() function is triggered. The
// driver must hide the mouser cursor while it is inside the shielding rectangle
// or the bounding box of the mouse cursor intersects the shielding rectangle.
//
// A HID display which implements a hardware accelerated mouse cursor does not
// need to implement cursor shielding and it can completely ignore teh shielding
// functionality.
//
// A HID display driver does not need to do anything to the mouse cursor when
// the display mode changes. E.g. it does not need to reposition the mouse cursor
// if its hot spot is outside a new display mode. The HID manager observers
// display changes and it automatically repositions the mouse cursor as needed.
//
// A HID display manager should send a change signal only when the current 
// display mode changes and a change signal has been set by calling the
// setChangeSignal() function.
//
// A HID display is an exclusive device.
//
open_class(IOHIDDisplay, IODriver,
);
open_class_funcs(IOHIDDisplay, IODriver,

    //
    // Display
    //

    // Returns the width and height (in pixels) of the currently active display
    // configuration.
    // Override: Required
    // Default: Does nothing
    void (*getScreenResolution)(void* _Nonnull self, int16_t* _Nonnull pOutWidth, int16_t* _Nonnull pOutHeight);

    // Specifies a VP that should receive the signal 'signo' every time the
    // current display configuration changes. Turns notifications off if 'vp' is
    // NULL.
    // Override: Required
    // Default: Does nothing
    void (*setChangeSignal)(void* _Nonnull self, vcpu_t _Nullable vp, int signo);


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

    // Returns true if the mouse cursor is implemented in software and thus
    // requires shielding; false otherwise. The return value of this function
    // is constant. This means that it will not change even if the display
    // mode or cursor changes.
    // Override: Optional
    // Default: returns false
    bool (*isCursorShieldingRequired)(void* _Nonnull self);

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
    // Override: Optional
    // Default: Does nothing
    void (*shieldCursor)(void* _Nonnull self, int x, int y, int width, int height);
);


//
// Subclassers
//

#define IOHIDDisplay_GetScreenResolution(__self, __w, __h) \
invoke_n(getScreenResolution, IOHIDDisplay, __self, __w, __h)

#define IOHIDDisplay_SetChangeSignal(__self, __vp, __signo) \
invoke_n(setChangeSignal, IOHIDDisplay, __self, __vp, __signo)


#define IOHIDDisplay_SetCursor(__self, __cursor) \
invoke_n(setCursor, IOHIDDisplay, __self, __cursor)

#define IOHIDDisplay_UpdateCursor(__self, __x, __y, __flags) \
invoke_n(updateCursor, IOHIDDisplay, __self, __x, __y, __flags)

#define IOHIDDisplay_IsCursorShieldingRequired(__self) \
invoke_0(isCursorShieldingRequired, IOHIDDisplay, __self)

#define IOHIDDisplay_ShieldCursor(__self, __x, __y, __width, __height) \
invoke_n(shieldCursor, IOHIDDisplay, __self, __x, __y, __width, __height)

#endif /* IOHIDDisplay_h */
