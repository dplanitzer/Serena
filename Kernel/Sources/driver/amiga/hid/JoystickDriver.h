//
//  JoystickDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef JoystickDriver_h
#define JoystickDriver_h

#include <klib/klib.h>
#include <driver/hid/InputDriver.h>
#include <driver/hid/EventDriver.h>


final_class(DigitalJoystickDriver, InputDriver);

extern errno_t DigitalJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, DigitalJoystickDriverRef _Nullable * _Nonnull pOutSelf);


final_class(AnalogJoystickDriver, InputDriver);

extern errno_t AnalogJoystickDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, AnalogJoystickDriverRef _Nullable * _Nonnull pOutSelf);

#endif /* JoystickDriver_h */
