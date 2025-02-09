//
//  LogDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/9/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef LogDriver_h
#define LogDriver_h

#include <driver/Driver.h>


final_class(LogDriver, Driver);

extern errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* LogDriver_h */
