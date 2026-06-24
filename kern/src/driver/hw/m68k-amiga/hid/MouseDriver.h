//
//  MouseDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef MouseDriver_h
#define MouseDriver_h

#include <driver/hid/IOHIDDevice.h>


final_class(MouseDriver, IOHIDDevice);

extern errno_t MouseDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* MouseDriver_h */
