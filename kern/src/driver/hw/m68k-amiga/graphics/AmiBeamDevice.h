//
//  AmiBeamDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiBeamDevice_h
#define AmiBeamDevice_h

#include <driver/hid/IOHIDBeamDevice.h>


open_class(AmiBeamDevice, IOHIDBeamDevice,
);
open_class_funcs(AmiBeamDevice, IOHIDBeamDevice,
);

extern errno_t AmiBeamDevice_Create(AmiBeamDeviceRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiBeamDevice_h */
