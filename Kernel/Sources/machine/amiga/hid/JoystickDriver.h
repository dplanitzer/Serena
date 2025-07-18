//
//  JoystickDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef JoystickDriver_h
#define JoystickDriver_h

#include <driver/hid/InputDriver.h>


final_class(DigitalJoystickDriver, InputDriver);

extern errno_t DigitalJoystickDriver_Create(DriverRef _Nullable parent, int port, DriverRef _Nullable * _Nonnull pOutSelf);


final_class(AnalogJoystickDriver, InputDriver);

extern errno_t AnalogJoystickDriver_Create(DriverRef _Nullable parent, int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* JoystickDriver_h */
