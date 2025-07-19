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


open_class(FSChannel, IOChannel,
    FilesystemRef _Nonnull  fs;
);
open_class_funcs(FSChannel, IOChannel,
);


extern errno_t FSChannel_Create(Class* _Nonnull pClass, IOChannelOptions options, int channelType, unsigned int mode, FilesystemRef _Nonnull fs, IOChannelRef _Nullable * _Nonnull pOutSelf);

// Returns a weak reference to the filesystem at the other end of this I/O channel.
// The filesystem reference remains valid as long as the I/O channel remains open.
#define FSChannel_GetFilesystemAs(__self, __class) \
((__class##Ref)((FSChannelRef)(__self))->fs)

#endif /* FSChannel_h */
