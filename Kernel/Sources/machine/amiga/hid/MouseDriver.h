//
//  MouseDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/25/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef MouseDriver_h
#define MouseDriver_h

#include <driver/hid/InputDriver.h>


final_class(MouseDriver, InputDriver);

extern errno_t MouseDriver_Create(CatalogId parentDirId, int port, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* MouseDriver_h */
