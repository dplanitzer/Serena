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
#include <driver/Driver.h>
#include <driver/hid/EventDriver.h>


final_class(LightPenDriver, Driver);

extern errno_t LightPenDriver_Create(EventDriverRef _Nonnull pEventDriver, int port, LightPenDriverRef _Nullable * _Nonnull pOutSelf);

#endif /* LightPenDriver_h */
