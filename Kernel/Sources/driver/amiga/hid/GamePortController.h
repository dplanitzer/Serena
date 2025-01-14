//
//  GamePortController.h
//  kernel
//
//  Created by Dietmar Planitzer on 1/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef GamePortController_h
#define GamePortController_h

#include <klib/klib.h>
#include <driver/Driver.h>
#include <driver/hid/EventDriver.h>


final_class(GamePortController, Driver);

extern errno_t GamePortController_Create(EventDriverRef _Nonnull pEventDriver, GamePortControllerRef _Nullable * _Nonnull pOutSelf);

#endif /* GamePortController_h */
