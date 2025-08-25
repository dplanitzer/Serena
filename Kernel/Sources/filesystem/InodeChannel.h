//
//  InodeChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/31/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef InodeChannel_h
#define InodeChannel_h

#include <filesystem/IOChannel.h>
#include <filesystem/Inode.h>


open_class(InodeChannel, IOChannel,
    InodeRef _Nonnull   inode;
);
open_class_funcs(InodeChannel, IOChannel,
);


// Creates a file object.
extern errno_t InodeChannel_Create(InodeRef _Nonnull pNode, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutFile);

#define InodeChannel_GetInode(__self) \
((InodeChannelRef)(__self))->inode

#endif /* InodeChannel_h */
