//
//  PaddleDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef PaddleDriver_h
#define PaddleDriver_h

#include <driver/hid/IOHIDDevice.h>


final_class(PaddleDriver, IOHIDDevice);

extern errno_t PaddleDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* PaddleDriver_h */
