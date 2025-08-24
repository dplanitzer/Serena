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


// Note that a process channel maintains a weak link to its process. The reason
// for this is that we want to ensure that if process X pens a channel to process
// Y, that X is not going to keep process Y alive simply by keeping the channel
// open for an arbitrary amount of time. The lifetime of process Y is independent
// of X. Thus process Y may go away at any time even if process X has a channel
// open to Y. Process X will simply receive a suitable error (ESRCH) when it
// tries to execute a function on the channel and process Y no longer exists.
open_class(ProcChannel, IOChannel,
    pid_t   target_pid;
);
open_class_funcs(ProcChannel, IOChannel,
);


extern errno_t ProcChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, pid_t targetPid, IOChannelRef _Nullable * _Nonnull pOutSelf);

#endif /* ProcChannel_h */
