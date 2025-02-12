//
//  PipeChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PipeChannel_h
#define PipeChannel_h

#include <filesystem/IOChannel.h>
#include <ipc/Pipe.h>


open_class(PipeChannel, IOChannel,
    PipeRef _Nonnull  pipe;
);
open_class_funcs(PipeChannel, IOChannel,
);


extern errno_t PipeChannel_Create(PipeRef _Nonnull pipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf);

#endif /* PipeChannel_h */
