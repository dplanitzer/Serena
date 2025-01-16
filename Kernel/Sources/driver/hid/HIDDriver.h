//
//  HIDDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/31/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDDriver_h
#define HIDDriver_h

#include <driver/Driver.h>


final_class(HIDDriver, Driver);

extern errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* HIDDriver_h */
