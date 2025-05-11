//
//  ProcChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 5/11/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef ProcChannel_h
#define ProcChannel_h

#include <filesystem/IOChannel.h>
#include <dispatcher/Lock.h>


open_class(ProcChannel, IOChannel,
    ProcessRef _Nonnull proc;
);
open_class_funcs(ProcChannel, IOChannel,
);


extern errno_t ProcChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, ProcessRef _Nonnull proc, IOChannelRef _Nullable * _Nonnull pOutSelf);

// Returns a weak reference to the process at the other end of this I/O channel.
// The process reference remains valid as long as the I/O channel remains open.
#define ProcChannel_GetProcess(__self) \
((ProcessRef)((ProcChannelRef)(__self))->proc)

#endif /* ProcChannel_h */
