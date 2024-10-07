//
//  EventChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef EventChannel_h
#define EventChannel_h

#include <driver/DriverChannel.h>


open_class(EventChannel, DriverChannel,
    TimeInterval    timeout;
);
open_class_funcs(EventChannel, DriverChannel,
);


extern errno_t EventChannel_Create(EventDriverRef _Nonnull pDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf);

#endif /* EventChannel_h */
