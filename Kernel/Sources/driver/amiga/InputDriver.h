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
#include <driver/Driver.h>
#include <driver/hid/EventDriver.h>


//
// Keyboard input driver
//

final_class(KeyboardDriver, Driver);

extern errno_t KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, KeyboardDriverRef _Nullable * _Nonnull pOutSelf);

extern void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay);


//
// Mouse input driver
//

final_class(MouseDriver, Driver);

extern errno_t MouseDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, MouseDriverRef _Nullable * _Nonnull pOutSelf);


//
// Digital Joystick input driver
//

final_class(DigitalJoystickDriver, Driver);

extern errno_t DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutSelf);


//
// Analog Joystick (Paddles) input driver
//

final_class(AnalogJoystickDriver, Driver);


extern errno_t AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutSelf);


//
// Light Pen input driver
//

final_class(LightPenDriver, Driver);

extern errno_t LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, LightPenDriverRef _Nullable * _Nonnull pOutSelf);

#endif /* InputDriver_h */
