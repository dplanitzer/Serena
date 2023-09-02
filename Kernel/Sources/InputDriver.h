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

typedef struct _KeyboardReport {
    HIDKeyCode  keycode;    // USB keycode
    Bool        isKeyUp;
} KeyboardReport;


extern ErrorCode KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutDriver);
extern void KeyboardDriver_Destroy(KeyboardDriverRef _Nullable pDriver);

extern Bool KeyboardDriver_GetReport(KeyboardDriverRef _Nonnull pDriver, KeyboardReport* _Nonnull pReport);



//
// Mouse input driver
//

struct _MouseDriver;
typedef struct _MouseDriver* MouseDriverRef;

typedef struct _MouseReport {
    Int16   xDelta;
    Int16   yDelta;
    UInt32  buttonsDown;    // L -> 0, R -> 1, M -> 2, ...
} MouseReport;


extern ErrorCode MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, MouseDriverRef _Nullable * _Nonnull pOutDriver);
extern void MouseDriver_Destroy(MouseDriverRef _Nullable pDriver);

extern Bool MouseDriver_GetReport(MouseDriverRef _Nonnull pDriver, MouseReport* _Nonnull pReport);



//
// Digital Joystick input driver
//

struct _DigitalJoystickDriver;
typedef struct _DigitalJoystickDriver* DigitalJoystickDriverRef;

typedef struct _JoystickReport {
    Int16   xAbs;           // Int16.min -> 100% left, 0 -> resting, Int16.max -> 100% right
    Int16   yAbs;           // Int16.min -> 100% up, 0 -> resting, Int16.max -> 100% down
    UInt32  buttonsDown;    // Button #0 -> 0, Button #1 -> 1, ...
} JoystickReport;


extern ErrorCode DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutDriver);
extern void DigitalJoystickDriver_Destroy(DigitalJoystickDriverRef _Nullable pDriver);

extern Bool DigitalJoystickDriver_GetReport(DigitalJoystickDriverRef _Nonnull pDriver, JoystickReport* _Nonnull pReport);



//
// Analog Joystick (Paddles) input driver
//

struct _AnalogJoystickDriver;
typedef struct _AnalogJoystickDriver* AnalogJoystickDriverRef;

extern ErrorCode AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutDriver);
extern void AnalogJoystickDriver_Destroy(AnalogJoystickDriverRef _Nullable pDriver);

extern Bool AnalogJoystickDriver_GetReport(AnalogJoystickDriverRef _Nonnull pDriver, JoystickReport* _Nonnull pReport);



//
// Light Pen input driver
//

struct _LightPenDriver;
typedef struct _LightPenDriver* LightPenDriverRef;

typedef struct _LightPenReport {
    Int16   xAbs;
    Int16   yAbs;
    UInt32  buttonsDown;    // Button #0 -> 0, Button #1 -> 1, ...
    Bool    hasPosition;    // true if the light pen position is available (pen triggered the position latching hardware); false otherwise
} LightPenReport;


extern ErrorCode LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, Int port, LightPenDriverRef _Nullable * _Nonnull pOutDriver);
extern void LightPenDriver_Destroy(LightPenDriverRef _Nullable pDriver);

extern Bool LightPenDriver_GetReport(LightPenDriverRef _Nonnull pDriver, LightPenReport* _Nonnull pReport);

#endif /* InputDriver_h */
