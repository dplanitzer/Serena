//
//  AmiKeyboard.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiKeyboard_h
#define AmiKeyboard_h

#include <driver/hid/IOHIDDevice.h>


final_class(AmiKeyboard, IOHIDDevice);

extern errno_t AmiKeyboard_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiKeyboard_h */
