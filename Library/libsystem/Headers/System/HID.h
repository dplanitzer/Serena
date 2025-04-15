//
//  HID.h
//  libsystem
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_HID_H
#define _SYS_HID_H 1

#include <System/Driver.h>

__CPP_BEGIN

// Types of input drivers
enum InputType {
    kInputType_None = 0,
    kInputType_Keyboard,
    kInputType_Keypad,
    kInputType_Mouse,
    kInputType_Trackball,
    kInputType_DigitalJoystick,
    kInputType_AnalogJoystick,
    kInputType_LightPen,
};
typedef int InputType;


#define kMouseCursor_Width  16
#define kMouseCursor_Height 16
#define kMouseCursor_PixelFormat    kPixelFormat_RGB_Indexed2


enum MouseCursorVisibility {
    kMouseCursor_Hidden,
    kMouseCursor_HiddenUntilMove,
    kMouseCursor_Visible
};
typedef int MouseCursorVisibility;


//
// HID Manager
//

// Dequeues and returns the next pending event from the event queue. Waits until
// an event arrives if none is pending and timeout is > 0. Returns ETIMEDOUT if
// no event has arrived before timeout. Returns EAGAIN if timeout is 0 and no
// event is pending. Note that this call disregards the kOpen_NonBlocking mode
// on the I/O channel.
// get_next_event(TimeInterval timeout, HIDEvent* _Nonnull evt)
#define kHIDCommand_GetNextEvent IOResourceCommand(kDriverCommand_SubclassBase + 0)


// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
#define kHIDCommand_GetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 1)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(TimeInterval initialDelay, TimeInterval repeatDelay)
#define kHIDCommand_SetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 2)


// Set the mouse cursor image.
// set_mouse_cursor(const uint16_t* _Nonnull planes[2], int width int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY)
#define kHIDCommand_SetMouseCursor IOResourceCommand(kDriverCommand_SubclassBase + 3)

// Changes the mouse cursor visibility to visible, hidden altogether or hidden
// until the user moves the mouse cursor. Note that the visibility state is
// absolute - nesting of calls isn't supported in this sense. Also note that the
// mouse cursor is hidden by default. You need to set a mouse cursor and then
// make it visible before it will show up on the screen.
// set_mouse_cursor_visibility(MouseCursorVisibility mode)
#define kHIDCommand_SetMouseCursorVisibility    IOResourceCommand(kDriverCommand_SubclassBase + 4)

// Returns the current mouse cursor visibility status.
// MouseCursorVisibility get_mouse_cursor_visibility(void)
#define kHIDCommand_GetMouseCursorVisibility    IOResourceCommand(kDriverCommand_SubclassBase + 5)

// Shields the mouse cursor. Call this function before drawing into the provided
// rectangular on the screen to ensure that the mouse cursor image will be saved
// and restored as needed.
// shield_mouse_cursor(int x, int y, int width, int height)
#define kHIDCommand_ShieldMouseCursor    IOResourceCommand(kDriverCommand_SubclassBase + 6)

// Unshield the mouse cursor and makes it visible again if it was visible before
// shielding. Call this function after you are done drawing to the screen.
// int unshield_mouse_cursor()
#define kHIDCommand_UnshieldMouseCursor  IOResourceCommand(kDriverCommand_SubclassBase + 7)


// Returns the type of input device for a port. There are two port: 0 and 1.
// get_port_device(int port, InputType* _Nonnull pOutType)
#define kHIDCommand_GetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 8)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, InputType type)
#define kHIDCommand_SetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 9)



//
// Raw Input Drivers
//

// Returns information about an input driver.
// get_info(InputInfo* _Nonnull pOutInfo)
#define kInputCommand_GetInfo   IOResourceCommand(kDriverCommand_SubclassBase + 0)

typedef struct InputInfo {
    InputType   inputType;      // The kind of input device
} InputInfo;


//
// Keyboard
//

// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
#define kKeyboardCommand_GetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(TimeInterval initialDelay, TimeInterval repeatDelay)
#define kKeyboardCommand_SetKeyRepeatDelays IOResourceCommand(kDriverCommand_SubclassBase + 1)


//
// GamePort Controller
//

// Returns the type of input device for a port. There are two port: 0 and 1.
// get_port_device(int port, InputType* _Nonnull pOutType)
#define kGamePortCommand_GetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 0)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, InputType type)
#define kGamePortCommand_SetPortDevice  IOResourceCommand(kDriverCommand_SubclassBase + 1)

__CPP_END

#endif /* _SYS_HID_H */
