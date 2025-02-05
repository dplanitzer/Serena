//
//  HID.h
//  libsystem
//
//  Created by Dietmar Planitzer on 1/12/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _SYS_HID_H
#define _SYS_HID_H 1

#include <System/IOChannel.h>

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

// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
#define kHIDCommand_GetKeyRepeatDelays IOResourceCommand(0)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(TimeInterval initialDelay, TimeInterval repeatDelay)
#define kHIDCommand_SetKeyRepeatDelays IOResourceCommand(1)


// Set the mouse cursor image.
// set_mouse_cursor(const uint16_t* _Nonnull planes[2], int width int height, PixelFormat pixelFormat, int hotSpotX, int hotSpotY)
#define kHIDCommand_SetMouseCursor IOResourceCommand(2)

// Changes the mouse cursor visibility to visible, hidden altogether or hidden
// until the user moves the mouse cursor. Note that the visibility state is
// absolute - nesting of calls isn't supported in this sense. Also note that the
// mouse cursor is hidden by default. You need to set a mouse cursor and then
// make it visible before it will show up on the screen.
// set_mouse_cursor_visibility(MouseCursorVisibility mode)
#define kHIDCommand_SetMouseCursorVisibility    IOResourceCommand(3)

// Returns the current mouse cursor visibility status.
// MouseCursorVisibility get_mouse_cursor_visibility(void)
#define kHIDCommand_GetMouseCursorVisibility    IOResourceCommand(4)

// Shields the mouse cursor. Call this function before drawing into the provided
// rectangular on the screen to ensure that the mouse cursor image will be saved
// and restored as needed.
// shield_mouse_cursor(int x, int y, int width, int height)
#define kHIDCommand_ShieldMouseCursor    IOResourceCommand(5)

// Unshield the mouse cursor and makes it visible again if it was visible before
// shielding. Call this function after you are done drawing to the screen.
// int unshield_mouse_cursor()
#define kHIDCommand_UnshieldMouseCursor  IOResourceCommand(6)


// Returns the type of input device for a port. There are two port: 0 and 1.
// get_port_device(int port, InputType* _Nonnull pOutType)
#define kHIDCommand_GetPortDevice  IOResourceCommand(7)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, InputType type)
#define kHIDCommand_SetPortDevice  IOResourceCommand(8)



//
// Raw Input Drivers
//

// Returns information about an input driver.
// get_info(InputInfo* _Nonnull pOutInfo)
#define kInputCommand_GetInfo   IOResourceCommand(0)

typedef struct InputInfo {
    InputType   inputType;      // The kind of input device
} InputInfo;


//
// Keyboard
//

// Returns the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// get_key_repeat_delays(TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay)
#define kKeyboardCommand_GetKeyRepeatDelays IOResourceCommand(0)

// Sets the initial delay for automatic key repeats and the delay between
// successive synthesized key presses.
// set_key_repeat_delays(TimeInterval initialDelay, TimeInterval repeatDelay)
#define kKeyboardCommand_SetKeyRepeatDelays IOResourceCommand(1)


//
// GamePort Controller
//

// Returns the type of input device for a port. There are two port: 0 and 1.
// get_port_device(int port, InputType* _Nonnull pOutType)
#define kGamePortCommand_GetPortDevice  IOResourceCommand(0)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, InputType type)
#define kGamePortCommand_SetPortDevice  IOResourceCommand(1)

__CPP_END

#endif /* _SYS_HID_H */
