//
//  JoystickDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef JoystickDriver_h
#define JoystickDriver_h

#include <driver/hid/IOHIDDevice.h>


final_class(JoystickDriver, IOHIDDevice);

extern errno_t JoystickDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* JoystickDriver_h */
