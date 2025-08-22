//
//  LogDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef LogDriver_h
#define LogDriver_h

#include <driver/pseudo/PseudoDriver.h>


final_class(LogDriver, PseudoDriver);
class_ref(LogDriver);

extern errno_t LogDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* LogDriver_h */
