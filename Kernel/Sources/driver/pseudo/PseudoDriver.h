//
//  PseudoDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/21/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef PseudoDriver_h
#define PseudoDriver_h

#include <driver/Driver.h>


open_class(PseudoDriver, Driver,
);
open_class_funcs(PseudoDriver, Driver,
);
class_ref(PseudoDriver);

extern errno_t PseudoDriver_Create(Class* _Nonnull pClass, unsigned int options, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* PseudoDriver_h */
