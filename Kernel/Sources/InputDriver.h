//
//  InputDriver.h
//  Apollo
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef InputDriver_h
#define InputDriver_h

#include <klib/klib.h>
#include "EventDriver.h"


//
// Keyboard input driver
//

struct _KeyboardDriver;
typedef struct _KeyboardDriver* KeyboardDriverRef;

extern ErrorCode KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutDriver);
extern void KeyboardDriver_Destroy(KeyboardDriverRef _Nullable pDriver);

extern void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay);


//
// Mouse input driver
//

struct _MouseDriver;
typedef struct _MouseDriver* MouseDriverRef;

extern ErrorCode MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, MouseDriverRef _Nullable * _Nonnull pOutDriver);
extern void MouseDriver_Destroy(MouseDriverRef _Nullable pDriver);


//
// Digital Joystick input driver
//

struct _DigitalJoystickDriver;
typedef struct _DigitalJoystickDriver* DigitalJoystickDriverRef;

extern ErrorCode DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutDriver);
extern void DigitalJoystickDriver_Destroy(DigitalJoystickDriverRef _Nullable pDriver);


//
// Analog Joystick (Paddles) input driver
//

struct _AnalogJoystickDriver;
typedef struct _AnalogJoystickDriver* AnalogJoystickDriverRef;

extern ErrorCode AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutDriver);
extern void AnalogJoystickDriver_Destroy(AnalogJoystickDriverRef _Nullable pDriver);


//
// Light Pen input driver
//

struct _LightPenDriver;
typedef struct _LightPenDriver* LightPenDriverRef;

extern ErrorCode LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, LightPenDriverRef _Nullable * _Nonnull pOutDriver);
extern void LightPenDriver_Destroy(LightPenDriverRef _Nullable pDriver);

#endif /* InputDriver_h */
