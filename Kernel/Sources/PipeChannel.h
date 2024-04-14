//
//  PipeChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PipeChannel_h
#define PipeChannel_h

#include <IOChannel.h>
#include <kobj/Object.h>


open_class_with_ref(PipeChannel, IOChannel,
    ObjectRef   pipe;
);
open_class_funcs(PipeChannel, IOChannel,
);


extern errno_t PipeChannel_Create(ObjectRef _Nonnull pPipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* PipeChannel_h */
