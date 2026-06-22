//
//  NullDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef NullDriver_h
#define NullDriver_h

#include <driver/Driver.h>


final_class(NullDriver, Driver);

extern errno_t NullDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* NullDriver_h */
