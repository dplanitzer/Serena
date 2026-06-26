//
//  AmiPaddle.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiPaddle_h
#define AmiPaddle_h

#include <driver/hid/IOHIDDevice.h>


final_class(AmiPaddle, IOHIDDevice);

extern errno_t AmiPaddle_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiPaddle_h */
