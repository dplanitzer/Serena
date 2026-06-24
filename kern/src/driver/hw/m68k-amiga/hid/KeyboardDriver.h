//
//  KeyboardDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef KeyboardDriver_h
#define KeyboardDriver_h

#include <driver/hid/IOHIDDevice.h>


final_class(KeyboardDriver, IOHIDDevice);

extern errno_t KeyboardDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* KeyboardDriver_h */
