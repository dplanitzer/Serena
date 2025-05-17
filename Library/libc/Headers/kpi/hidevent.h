//
//  kpi/hidevent.h
//  libc
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef _KPI_HID_EVENT_H
#define _KPI_HID_EVENT_H 1

#include <stdbool.h>
#include <stdint.h>
#include <kpi/_time.h>


// Event types
typedef enum HIDEventType {
    kHIDEventType_KeyDown = 0,
    kHIDEventType_KeyUp,
    kHIDEventType_FlagsChanged,
    kHIDEventType_MouseDown,
    kHIDEventType_MouseUp,
    kHIDEventType_MouseMoved,
    kHIDEventType_JoystickDown,
    kHIDEventType_JoystickUp,
    kHIDEventType_JoystickMotion
} HIDEventType;


// Modifier key flags
// flags uint32_t encoding:
// [15...0]: logical modifier flags
// [23...16]: right shift / control / option / command pressed
// [31...24]: left shift / control / option / command pressed
enum {
    kHIDEventModifierFlag_Shift         = 1,    // Any shift key except caps lock is pressed
    kHIDEventModifierFlag_Option        = 2,    // Any option key is pressed
    kHIDEventModifierFlag_Control       = 4,    // Any control key is pressed
    kHIDEventModifierFlag_Command       = 8,    // Any command / GUI key is pressed
    kHIDEventModifierFlag_CapsLock      = 16,   // Caps lock key is pressed
    kHIDEventModifierFlag_KeyPad        = 32,   // Any key on the key pad is pressed
    kHIDEventModifierFlag_Function      = 64,   // Any function key is pressed (this includes literal function 'F' keys and cursor keys, return, delete, etc)
};


// HID key codes are based on the USB HID key scan codes
typedef uint16_t  HIDKeyCode;


// HID event data
typedef struct HIDEventData_KeyUpDown {
    uint32_t    flags;          // Modifier keys
    HIDKeyCode  keyCode;        // USB HID key scan code
    bool        isRepeat;       // True if this is an auto-repeated key down; false otherwise
} HIDEventData_KeyUpDown;


typedef struct HIDEventData_FlagsChanged {
    uint32_t    flags;              // Modifier keys
} HIDEventData_FlagsChanged;


typedef struct HIDEventData_MouseButton {
    int         buttonNumber;   // 0 -> left button, 1 -> right button, 2-> middle button, ...
    uint32_t    flags;          // modifier keys
    int         x;              // Mouse position when the button was pressed / released
    int         y;
} HIDEventData_MouseButton;


typedef struct HIDEventData_MouseMove {
    int         x;              // Current mouse position
    int         y;
    uint32_t    flags;          // Modifier keys
} HIDEventData_MouseMove;


typedef struct HIDEventData_JoystickButton {
    int         port;           // Input controller port number
    int         buttonNumber;
    uint32_t    flags;          // Modifier keys
    int         dx;             // Joystick direction when the button was pressed / released
    int         dy;
} HIDEventData_JoystickButton;


typedef struct HIDEventData_JoystickMotion {
    int         port;           // Input controller port number
    int         dx;
    int         dy;
} HIDEventData_JoystickMotion;


typedef union _HIDEventData {
    HIDEventData_KeyUpDown      key;
    HIDEventData_FlagsChanged   flags;
    HIDEventData_MouseButton    mouse;
    HIDEventData_MouseMove      mouseMoved;
    HIDEventData_JoystickButton joystick;
    HIDEventData_JoystickMotion joystickMotion;
} HIDEventData;


// HID event
typedef struct HIDEvent {
    int32_t         type;
    struct timespec eventTime;
    HIDEventData    data;
} HIDEvent;


// HID key state
typedef enum HIDKeyState {
    kHIDKeyState_Down,
    kHIDKeyState_Repeat,
    kHIDKeyState_Up
} HIDKeyState;

#endif /* _KPI_HID_EVENT_H */
