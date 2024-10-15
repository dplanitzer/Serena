//
//  EventDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef EventDriver_h
#define EventDriver_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <driver/amiga/graphics/GraphicsDriver.h>
#include "HIDEvent.h"

extern const char* const kEventDriverName;


final_class(EventDriver, Driver);


// Input controller types
typedef enum InputControllerType {
    kInputControllerType_None = 0,      // No input controller configured for the port
    kInputControllerType_Mouse,
    kInputControllerType_DigitalJoystick,
    kInputControllerType_AnalogJoystick,
    kInputControllerType_LightPen
} InputControllerType;


// HID key state
typedef enum HIDKeyState {
    kHIDKeyState_Down,
    kHIDKeyState_Repeat,
    kHIDKeyState_Up
} HIDKeyState;


extern errno_t EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutSelf);


// APIs for use by input drivers
extern GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull self);

extern void EventDriver_ReportKeyboardDeviceChange(EventDriverRef _Nonnull self, HIDKeyState keyState, uint16_t keyCode);
extern void EventDriver_ReportMouseDeviceChange(EventDriverRef _Nonnull self, int16_t xDelta, int16_t yDelta, uint32_t buttonsDown);
extern void EventDriver_ReportJoystickDeviceChange(EventDriverRef _Nonnull self, int port, int16_t xAbs, int16_t yAbs, uint32_t buttonsDown);
extern void EventDriver_ReportLightPenDeviceChange(EventDriverRef _Nonnull self, int16_t xAbs, int16_t yAbs, bool hasPosition, uint32_t buttonsDown);


// Configuring the HID assignment of HID ports to HID drivers
extern InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull self, int portId);
extern errno_t EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull self, InputControllerType type, int portId);


// Configuring the keyboard
extern void EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay);


// Returns the keyboard hardware state
extern void EventDriver_GetDeviceKeysDown(EventDriverRef _Nonnull self, const HIDKeyCode* _Nullable pKeysToCheck, int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, int* _Nonnull nKeysDown);


// The mouse cursor state
extern void EventDriver_SetMouseCursor(EventDriverRef _Nonnull self, const void* pBitmap, const void* pMask);
extern void EventDriver_ShowMouseCursor(EventDriverRef _Nonnull self);
extern void EventDriver_HideMouseCursor(EventDriverRef _Nonnull self);
extern void EventDriver_SetMouseCursorHiddenUntilMouseMoves(EventDriverRef _Nonnull self, bool flag);

// Returns the mouse hardware state
extern Point EventDriver_GetMouseDevicePosition(EventDriverRef _Nonnull self);
extern uint32_t EventDriver_GetMouseDeviceButtonsDown(EventDriverRef _Nonnull self);


// Getting events
// Returns events in the order oldest to newest. As many events are returned as
// fit in the provided buffer. Only blocks the caller if no events are queued.
extern errno_t EventDriver_Read(EventDriverRef _Nonnull self, void* _Nonnull pBuffer, ssize_t nBytesToRead, TimeInterval timeout, ssize_t* _Nonnull nOutBytesRead);

#endif /* EventDriver_h */
