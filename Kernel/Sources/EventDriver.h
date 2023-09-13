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


extern ErrorCode EventDriver_Create(GraphicsDriverRef _Nonnull gdevice, EventDriverRef _Nullable * _Nonnull pOutDriver);
extern void EventDriver_Destroy(EventDriverRef _Nullable pDriver);

extern GraphicsDriverRef _Nonnull EventDriver_GetGraphicsDriver(EventDriverRef _Nonnull pDriver);

extern InputControllerType EventDriver_GetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, Int portId);
extern ErrorCode EventDriver_SetInputControllerTypeForPort(EventDriverRef _Nonnull pDriver, InputControllerType type, Int portId);

extern void EventDriver_GetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void EventDriver_SetKeyRepeatDelays(EventDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay);

extern void EventDriver_ShowMouseCursor(EventDriverRef _Nonnull pDriver);
extern void EventDriver_HideMouseCursor(EventDriverRef _Nonnull pDriver);
extern void EventDriver_ObscureMouseCursor(EventDriverRef _Nonnull pDriver);

extern Point EventDriver_GetMouseLocation(EventDriverRef _Nonnull pDriver);
extern UInt32 EventDriver_GetMouseButtonsDown(EventDriverRef _Nonnull pDriver);
extern void EventDriver_GetKeysDown(EventDriverRef _Nonnull pDriver, const HIDKeyCode* _Nullable pKeysToCheck, Int nKeysToCheck, HIDKeyCode* _Nullable pKeysDown, Int* _Nonnull nKeysDown);

extern ErrorCode EventDriver_GetEvents(EventDriverRef _Nonnull pDriver, HIDEvent* _Nonnull pEvents, Int* _Nonnull pEventCount, TimeInterval deadline);
extern void EventDriver_PostEvent(EventDriverRef _Nonnull pDriver, const HIDEvent* _Nonnull pEvent);

#endif /* EventDriver_h */
