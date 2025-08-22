//
//  HIDDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HIDDriver_h
#define HIDDriver_h

#include <driver/pseudo/PseudoDriver.h>


final_class(HIDDriver, PseudoDriver);
class_ref(HIDDriver);


extern errno_t HIDDriver_Create(DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* HIDDriver_h */
