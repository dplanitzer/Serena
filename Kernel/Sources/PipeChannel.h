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


OPEN_CLASS_WITH_REF(PipeChannel, IOChannel,
    ObjectRef   pipe;
);
typedef struct PipeChannelMethodTable {
    IOChannelMethodTable    super;
} PipeChannelMethodTable;


extern errno_t PipeChannel_Create(ObjectRef _Nonnull pPipe, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutChannel);

#endif /* PipeChannel_h */
