//
//  HIDEvent.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDEvent_h
#define HIDEvent_h

#include <klib/klib.h>


// Event types
typedef enum _HIDEventType {
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
// flags UInt32 encoding:
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
typedef UInt16  HIDKeyCode;


// HID event data
typedef struct _HIDEventData_KeyUpDown {
    UInt32      flags;          // Modifier keys
    HIDKeyCode  keyCode;        // USB HID keyscan code
    Bool        isRepeat;       // True if this is an auto-repeated key down; false otherwise
} HIDEventData_KeyUpDown;


typedef struct _HIDEventData_FlagsChanged {
    UInt32  flags;              // Modifier keys
} HIDEventData_FlagsChanged;


typedef struct _HIDEventData_MouseButton {
    Int         buttonNumber;   // 0 -> left button, 1 -> right button, 2-> middle button, ...
    UInt32      flags;          // modifier keys
    Point       location;       // Mouse position when the button was pressed / released
} HIDEventData_MouseButton;


typedef struct _HIDEventData_MouseMove {
    Point       location;       // Current mouse position
    UInt32      flags;          // Modifier keys
} HIDEventData_MouseMove;


typedef struct _HIDEventData_JoystickButton {
    Int         port;           // Input controller port number
    Int         buttonNumber;
    UInt32      flags;          // Modifier keys
    Vector      direction;      // Joystick direction when the button was pressed / released
} HIDEventData_JoystickButton;


typedef struct _HIDEventData_JoystickMotion {
    Int         port;           // Input controller port number
    Vector      direction;
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
typedef struct _HIDEvent {
    Int32           type;
    TimeInterval    eventTime;
    HIDEventData    data;
} HIDEvent;

#endif /* HIDEvent_h */
