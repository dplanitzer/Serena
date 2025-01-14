//
//  KeyboardDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef KeyboardDriver_h
#define KeyboardDriver_h

#include <klib/klib.h>
#include <driver/hid/InputDriver.h>


final_class(KeyboardDriver, InputDriver);

extern errno_t KeyboardDriver_Create(EventDriverRef _Nonnull pEventDriver, DriverRef _Nullable * _Nonnull pOutSelf);

extern void KeyboardDriver_GetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval* _Nullable pInitialDelay, TimeInterval* _Nullable pRepeatDelay);
extern void KeyboardDriver_SetKeyRepeatDelays(KeyboardDriverRef _Nonnull self, TimeInterval initialDelay, TimeInterval repeatDelay);

#endif /* KeyboardDriver_h */
