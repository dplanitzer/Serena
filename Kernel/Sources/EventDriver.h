//
//  EventDriver.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef EventDriver_h
#define EventDriver_h

#include <klib/klib.h>
#include "GraphicsDriver.h"
#include "HIDEvent.h"
#include "Resource.h"


struct _EventDriver;
typedef struct _EventDriver* EventDriverRef;


// Input controller types
typedef enum _InputControllerType {
    kInputControllerType_None = 0,      // No input controller configured for the port
    kInputControllerType_Mouse,
    kInputControllerType_DigitalJoystick,
    kInputControllerType_AnalogJoystick,
    kInputControllerType_LightPen
} InputControllerType;


// HID key state
typedef enum _HIDKeyState {
    kHIDKeyState_Down,
    kHIDKeyState_Repeat,
    kHIDKeyState_Up
} HIDKeyState;


extern ErrorCode EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutDriver);


// APIs for use by input drivers
extern GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver);

extern void EventDriver_ReportKeyboardDeviceChange(EventDriverRef _Nonnull pDriver, HIDKeyState keyState, UInt16 keyCode);
extern void EventDriver_ReportMouseDeviceChange(EventDriverRef _Nonnull pDriver, Int16 xDelta, Int16 yDelta, UInt32 buttonsDown);
extern void EventDriver_ReportJoystickDeviceChange(EventDriverRef _Nonnull pDriver, Int port, Int16 xAbs, Int16 yAbs, UInt32 buttonsDown);
extern void EventDriver_ReportLightPenDeviceChange(EventDriverRef _Nonnull pDriver, Int16 xAbs, Int16 yAbs, Bool hasPosition, UInt32 buttonsDown);


// Configuring the HID assignment of HID ports to HID drivers
extern InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, Int portId);
extern ErrorCode EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId);


// Configuring the keyboard
extern void EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay);


// Returns the keyboard hardware state
extern void EventDriver_GetDeviceKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, Int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, Int* _Nonnull nKeysDown);


// The mouse cursor state
extern void EventDriver_SetMouseCursor(EventDriverRef _Nonnull pDriver, const Byte* pBitmap, const Byte* pMask);
extern void EventDriver_ShowMouseCursor(EventDriverRef _Nonnull pDriver);
extern void EventDriver_HideMouseCursor(EventDriverRef _Nonnull pDriver);
extern void EventDriver_SetMouseCursorHiddenUntilMouseMoves(EventDriverRef _Nonnull pDriver, Bool flag);

// Returns the mouse hardware state
extern Point EventDriver_GetMouseDevicePosition(EventDriverRef _Nonnull pDriver);
extern UInt32 EventDriver_GetMouseDeviceButtonsDown(EventDriverRef _Nonnull pDriver);

#endif /* EventDriver_h */
