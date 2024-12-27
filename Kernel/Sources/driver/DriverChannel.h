//
//  DriverChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/4/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DriverChannel_h
#define DriverChannel_h

#include <filesystem/IOChannel.h>


typedef enum DriverChannelOptions {
    kDriverChannel_Seekable = 1     // Driver channel should allow seeking
} DriverChannelOptions;


open_class(DriverChannel, IOChannel,
    // XXX sort out locking (at least for offset; driver & options are constant anyway)
    FileOffset              offset;
    DriverRef _Nonnull      driver;
    DriverChannelOptions    options;
);
open_class_funcs(DriverChannel, IOChannel,

    // Invoked when the driver channel needs the size of the seekable space. The
    // maximum position to which a client is allowed to seek is this value minus
    // one.
    // Override: Optional
    // Default Behavior: Returns 0
    FileOffset (*getSeekableRange)(void* _Nonnull self);
);


extern errno_t DriverChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, DriverChannelOptions options, DriverRef _Nonnull pDriver, IOChannelRef _Nullable * _Nonnull pOutSelf);

#define DriverChannel_GetOffset(__self) \
((DriverChannelRef)(__self))->offset

// Returns the size of the seekable range
#define DriverChannel_GetSeekableRange(__self) \
invoke_0(getSeekableRange, DriverChannel, __self)

#endif /* DriverChannel_h */
