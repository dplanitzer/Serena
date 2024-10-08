//
//  DriverChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverChannel_h
#define DriverChannel_h

#include <IOChannel.h>


open_class(DriverChannel, IOChannel,
    DriverRef _Nonnull  driver;
);
open_class_funcs(DriverChannel, IOChannel,
);


extern errno_t DriverChannel_Create(Class* _Nonnull pClass, DriverRef _Nonnull pDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf);

#endif /* DriverChannel_h */
