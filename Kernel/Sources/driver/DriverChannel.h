//
//  DriverChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverChannel_h
#define DriverChannel_h

#include <filesystem/IOChannel.h>


open_class(DriverChannel, IOChannel,
    DriverRef _Nonnull  drv;
    void* _Nullable     extras;
);
open_class_funcs(DriverChannel, IOChannel,
);


extern errno_t DriverChannel_Create(DriverRef _Nonnull drv, int channelType, unsigned int mode, size_t nExtraBytes, IOChannelRef _Nullable * _Nonnull pOutChannel);

#define DriverChannel_GetDriverAs(__self, __class) \
((__class##Ref)((DriverChannelRef)__self)->drv)

#define DriverChannel_GetExtrasAs(__self, __type) \
((__type*)((DriverChannelRef)__self)->extras)

#endif /* DriverChannel_h */
