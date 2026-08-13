//
//  AGADriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/7/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef AGADriver_h
#define AGADriver_h

#include <driver/IOGraphicsDriver.h>

final_class(AGADriver, IOGraphicsDriver);

extern errno_t AGADriver_Create(AGADriverRef _Nullable * _Nonnull pOutSelf);

#endif /* AGADriver_h */
