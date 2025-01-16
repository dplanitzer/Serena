//
//  LightPenDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef LightPenDriver_h
#define LightPenDriver_h

#include <klib/klib.h>
#include <driver/hid/InputDriver.h>


final_class(LightPenDriver, InputDriver);

extern errno_t LightPenDriver_Create(int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* LightPenDriver_h */
