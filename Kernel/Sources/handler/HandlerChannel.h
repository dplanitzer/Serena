//
//  HandlerChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 8/3/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef HandlerChannel_h
#define HandlerChannel_h

#include <filesystem/IOChannel.h>


open_class(HandlerChannel, IOChannel,
    HandlerRef _Nonnull hnd;
    void* _Nullable     extras;
);
open_class_funcs(HandlerChannel, IOChannel,
);


extern errno_t HandlerChannel_Create(HandlerRef _Nonnull hnd, int channelType, unsigned int mode, size_t nExtraBytes, IOChannelRef _Nullable * _Nonnull pOutChannel);

#define HandlerChannel_GetHandlerAs(__self, __class) \
((__class##Ref)((HandlerChannelRef)__self)->hnd)

#define HandlerChannel_GetExtrasAs(__self, __type) \
((__type*)((HandlerChannelRef)__self)->extras)

#endif /* HandlerChannel_h */
