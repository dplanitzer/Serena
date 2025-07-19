//
//  DriverChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverChannel_h
#define DriverChannel_h

#include <filesystem/IOChannel.h>
#include <sched/mtx.h>


open_class(DriverChannel, IOChannel,
    mtx_t               mtx;
    DriverRef _Nonnull  driver;
);
open_class_funcs(DriverChannel, IOChannel,
);


extern errno_t DriverChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, DriverRef _Nonnull pDriver, IOChannelRef _Nullable * _Nonnull pOutSelf);

// Returns a weak reference to the driver at the other end of this I/O channel.
// The driver reference remains valid as long as the I/O channel remains open.
#define DriverChannel_GetDriverAs(__self, __class) \
((__class##Ref)((DriverChannelRef)(__self))->driver)

#endif /* DriverChannel_h */
