//
//  AmiMouse.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiMouse_h
#define AmiMouse_h

#include <driver/hid/IOHIDDevice.h>


final_class(AmiMouse, IOHIDDevice);

extern errno_t AmiMouse_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiMouse_h */
