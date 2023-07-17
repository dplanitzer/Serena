//
//  EventDriver.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef EventDriver_h
#define EventDriver_h

#include "Foundation.h"
#include "GraphicsDriver.h"
#include "MonotonicClock.h"


struct _EventDriver;
typedef struct _EventDriver* EventDriverRef;


// Event types
enum {
    kHIDEventType_KeyDown = 0,
    kHIDEventType_KeyUp,
    kHIDEventType_FlagsChanged,
    kHIDEventType_MouseDown,
    kHIDEventType_MouseUp,
    kHIDEventType_MouseMoved,
    kHIDEventType_JoystickDown,
    kHIDEventType_JoystickUp,
    kHIDEventType_JoystickMotion
};


// Modifier key flags
// flags UInt32 encoding:
// [15...0]: modifier summary flags
// [23...16]: right shift / control / option / command pressed
// [31...24]: left shift / control / option / command pressed
enum {
    kHIDEventModifierFlag_CapsLock      = 1,    // Caps lock key is pressed
    kHIDEventModifierFlag_Shift         = 2,    // Any shift key except caps lock is pressed
    kHIDEventModifierFlag_Control       = 4,    // Any control key is pressed
    kHIDEventModifierFlag_Option        = 8,    // Any option key is pressed
    kHIDEventModifierFlag_Command       = 16,   // Any command / GUI key is pressed
    kHIDEventModifierFlag_KeyPad        = 32,   // Any key on the key pad is pressed
    kHIDEventModifierFlag_Function      = 64,   // Any function key is pressed (this includes literal function 'F' keys and cursor keys, return, delete, etc)
};


// HID key codes are based on the USB HID key scan codes
typedef UInt16  HIDKeyCode;


// HID event data
typedef union _HIDEventData {
    struct _HIDEventData_KeyUpDown {
        UInt32      flags;          // modifier keys
        HIDKeyCode  keycode;        // USB HID keyscan code
        Bool        isRepeat;       // true if this is an auto-repeated key down; false otherwise
    }   key;
    
    struct _HIDEventData_FlagsChanged {
        UInt32  flags;              // modifier keys
    }   flags;
    
    struct _HIDEventData_MouseButton {
        Int         buttonNumber;   // 0 -> left button, 1 -> right button, 2-> middle button, ...
        UInt32      flags;          // modifier keys
        Point       location;       // Mouse position when the button was pressed / released
    }   mouse;
    
    struct _HIDEventData_MouseMove {
        Point       location;
    }   mouseMoved;
    
    struct _HIDEventData_JoystickButton {
        Int         port;           // Input controller port number
        Int         buttonNumber;
        UInt32      flags;          // modifier keys
        Vector      direction;      // Joystick direction when the button was pressed / released
    }   joystick;
    
    struct _HIDEventData_JoystickMotion {
        Int         port;           // Input controller port number
        Vector      direction;
    }   joystickMotion;
} HIDEventData;


// HID event
typedef struct _HIDEvent {
    Int32           type;
    TimeInterval    eventTime;
    HIDEventData    data;
} HIDEvent;


// Input controller types
typedef enum _InputControllerType {
    kInputControllerType_None = 0,      // No input controller configured for the port
    kInputControllerType_Mouse,
    kInputControllerType_DigitalJoystick,
    kInputControllerType_AnalogJoystick,
    kInputControllerType_LightPen
} InputControllerType;


extern ErrorCode EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutDriver);
extern void EventDriver_Destroy(EventDriverRef _Nullable pDriver);

extern GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver);

extern ErrorCode EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, Int portId, InputControllerType * _Nonnull pOutType);
extern ErrorCode EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId);

extern ErrorCode EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern ErrorCode EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay);

extern ErrorCode EventDriver_ShowMouseCursor(EventDriverRef _Nonnull pDriver);
extern ErrorCode EventDriver_HideMouseCursor(EventDriverRef _Nonnull pDriver);
extern ErrorCode EventDriver_ObscureMouseCursor(EventDriverRef _Nonnull pDriver);

extern ErrorCode EventDriver_GetMouseLocation(EventDriverRef _Nonnull pDriver, Point* _Nonnull pOutPoint);
extern ErrorCode EventDriver_GetMouseButtonsDown(EventDriverRef _Nonnull pDriver, UInt32* _Nonnull pOutButtonsMask);
extern void EventDriver_GetKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, Int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, Int* _Nonnull nKeysDown);

extern ErrorCode EventDriver_GetEvents(EventDriverRef _Nonnull pDriver, HIDEvent* _Nonnull pEvents, Int* _Nonnull pEventCount, TimeInterval deadline);
extern ErrorCode EventDriver_PostEvent(EventDriverRef _Nonnull pDriver, const HIDEvent* _Nonnull pEvent);

#endif /* EventDriver_h */
