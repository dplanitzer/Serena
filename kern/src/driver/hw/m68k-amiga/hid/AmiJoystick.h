//
//  AmiJoystick.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiJoystick_h
#define AmiJoystick_h

#include <driver/hid/IOHIDDevice.h>


final_class(AmiJoystick, IOHIDDevice);

extern errno_t AmiJoystick_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiJoystick_h */
