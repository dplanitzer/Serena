//
//  InputDriver.h
//  kernel
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

OPAQUE_CLASS(KeyboardDriver, IOResource);
typedef struct _KeyboardDriverMethodTable {
    IOResourceMethodTable   super;
} KeyboardDriverMethodTable;

extern errno_t KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutDriver);

extern void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull pDriver, TimeInterval initialDelay, TimeInterval repeatDelay);


//
// Mouse input driver
//

OPAQUE_CLASS(MouseDriver, IOResource);
typedef struct _MouseDriverMethodTable {
    IOResourceMethodTable   super;
} MouseDriverMethodTable;

extern errno_t MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, MouseDriverRef _Nullable * _Nonnull pOutDriver);


//
// Digital Joystick input driver
//

OPAQUE_CLASS(DigitalJoystickDriver, IOResource);
typedef struct _DigitalJoystickDriverMethodTable {
    IOResourceMethodTable   super;
} DigitalJoystickDriverMethodTable;

extern errno_t DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutDriver);


//
// Analog Joystick (Paddles) input driver
//

OPAQUE_CLASS(AnalogJoystickDriver, IOResource);
typedef struct _AnalogJoystickDriverMethodTable {
    IOResourceMethodTable   super;
} AnalogJoystickDriverMethodTable;


extern errno_t AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutDriver);


//
// Light Pen input driver
//

OPAQUE_CLASS(LightPenDriver, IOResource);
typedef struct _LightPenDriverMethodTable {
    IOResourceMethodTable   super;
} LightPenDriverMethodTable;

extern errno_t LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, LightPenDriverRef _Nullable * _Nonnull pOutDriver);

#endif /* InputDriver_h */
