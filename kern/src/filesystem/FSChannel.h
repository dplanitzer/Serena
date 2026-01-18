//
//  FSChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 4/13/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef FSChannel_h
#define FSChannel_h

#include <filesystem/IOChannel.h>


// Resource: FilesystemRef
open_class(FSChannel, IOChannel,
);
open_class_funcs(FSChannel, IOChannel,
);


extern errno_t FSChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, FilesystemRef _Nonnull fs, IOChannelRef _Nullable * _Nonnull pOutSelf);

#endif /* FSChannel_h */
