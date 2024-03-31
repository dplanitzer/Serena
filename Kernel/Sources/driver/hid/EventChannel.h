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


OPEN_CLASS_WITH_REF(EventChannel, IOChannel,
    ObjectRef       eventDriver;
    TimeInterval    timeout;
);
typedef struct _EventChannelMethodTable {
    IOChannelMethodTable    super;
} EventChannelMethodTable;


extern errno_t EventChannel_Create(ObjectRef _Nonnull pEventDriver, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* EventChannel_h */
