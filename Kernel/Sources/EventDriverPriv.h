//
//  EventDriverPriv.h
//  Apollo
//
//  Created by Dietmar Planitzer on 10/04/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef EventDriverPriv_h
#define EventDriverPriv_h

#include "EventDriver.h"
#include "HIDEventQueue.h"
#include "InputDriver.h"
#include "Lock.h"
#include "MonotonicClock.h"
#include "USBHIDKeys.h"


// XXX 16 is confirmed to work without overflows on a A2000. Still want to keep
// 48 for now for mouse move. Though once we support coalescing we may want to
// revisit this.
#define REPORT_QUEUE_MAX_EVENTS    48
#define MAX_INPUT_CONTROLLER_PORTS  2
#define KEY_MAP_INTS_COUNT          (256/32)


// State of a joystick device
typedef struct _LogicalJoystick {
    Int16   xAbs;           // Int16.min -> 100% left, 0 -> resting, Int16.max -> 100% right
    Int16   yAbs;           // Int16.min -> 100% up, 0 -> resting, Int16.max -> 100% down
    UInt32  buttonsDown;    // Button #0 -> 0, Button #1 -> 1, ...
} LogicalJoystick;


// Per port input controller state
typedef struct _InputControllerState {
    InputControllerType     type;
    IOResourceRef _Nullable driver;
} InputControllerState;


// Explanation of logical keyboard/mouse device:
//
// The event driver maintains a logical keyboard and mouse device. These devices
// reflect the current state of the hardware as closely as possible and with as
// little latency as possible. So this state is maintained before the event queue.
// However these devices are logical in the sense that multiple hardware devices
// may contribute to their state. Eg multiple keyboards may contribute to the
// logical keyboard and multiple mice and other devices such as a joystick or
// light pen may contribute to the state of the logical mouse. 
CLASS_IVARS(EventDriver, IOResource,
    Lock                        lock;
    GraphicsDriverRef _Nonnull  gdevice;
    HIDEventQueueRef _Nonnull   eventQueue;
    KeyboardDriverRef _Nonnull  keyboardDriver;
    InputControllerState        port[MAX_INPUT_CONTROLLER_PORTS];

    // Keyboard Configuration
    const UInt8*                keyFlags;


    // Mouse Configuration
    Int16                       screenLeft;
    Int16                       screenTop;
    Int16                       screenRight;
    Int16                       screenBottom;
    Int                         mouseCursorHiddenCounter;
    Bool                        isMouseMoveReportingEnabled;    // true if position-change-only mouse reports should be queued; false if we only care about mouse button changes


    // Logical Keyboard Device
    //
    // XXX A word on the key map:
    // The embedded map here is just a note for the future. In the future the map
    // will be in a sharable page. Apps will then be able to map that page read-only
    // via an iocall. The app will then be able to copy/scan the map as needed while
    // the input keyboard driver updates it.
    UInt32                      keyMap[KEY_MAP_INTS_COUNT];    // keycode is the bit index. 1 -> key down; 0 -> key up
    UInt32                      modifierFlags;


    // Logical Mouse Device
    //
    Int16                       mouseX;
    Int16                       mouseY;
    UInt32                      mouseButtons;


    // Logical Joystick Devices
    LogicalJoystick             joystick[MAX_INPUT_CONTROLLER_PORTS];
);


// Event Driver I/O Channel
OPEN_CLASS_WITH_REF(EventDriverChannel, IOChannel,
    TimeInterval    timeout;
);
typedef struct _EventDriverChannelMethodTable {
    IOChannelMethodTable    super;
} EventDriverChannelMethodTable;


extern ErrorCode EventDriver_CreateInputControllerForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId);
extern void EventDriver_DestroyInputControllerForPort(EventDriverRef _Nonnull pDriver, Int portId);

#endif /* EventDriverPriv_h */
