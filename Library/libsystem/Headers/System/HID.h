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


// Show the mouse cursor. This increments the mouse-cursor-hidden counter. The
// cursor becomes visible once the counter reaches 0.
// show_mouse_cursor(void)
#define kHIDCommand_ShowMouseCursor IOResourceCommand(2)

// Hide the mouse cursor. This decrements the mouse-cursor-hidden counter. The
// cursor becomes invisible once the counter reaches < 0.
// hide_mouse_cursor(void)
#define kHIDCommand_HideMouseCursor IOResourceCommand(3)

// Marks the mouse cursor as hidden until the user moves the mouse.
// set_mouse_cursor_hidden_until_move(bool flag)
#define kHIDCommand_SetMouseCursorHiddenUntilMove IOResourceCommand(4)


// Returns the type of input device for a port. There are two port: 0 and 1.
// get_port_device(int port, InputType* _Nonnull pOutType)
#define kHIDCommand_GetPortDevice  IOResourceCommand(5)

// Selects the type of input device for a port. There are two port: 0 and 1.
// set_port_device(int port, InputType type)
#define kHIDCommand_SetPortDevice  IOResourceCommand(6)



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
