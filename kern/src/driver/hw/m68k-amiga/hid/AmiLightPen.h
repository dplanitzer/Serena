//
//  AmiLightPen.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef AmiLightPen_h
#define AmiLightPen_h

#include <driver/hid/IOHIDDevice.h>


final_class(AmiLightPen, IOHIDDevice);

extern errno_t AmiLightPen_Create(int port, IODriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AmiLightPen_h */
