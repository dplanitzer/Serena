//
//  EventChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef EventChannel_h
#define EventChannel_h

#include <IOChannel.h>


open_class_with_ref(EventChannel, IOChannel,
    ObjectRef       eventDriver;
    TimeInterval    timeout;
);
open_class_funcs(EventChannel, IOChannel,
);


extern errno_t EventChannel_Create(ObjectRef _Nonnull pEventDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* EventChannel_h */
