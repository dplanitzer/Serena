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
);


extern errno_t DriverChannel_Create(Class* _Nonnull pClass, int channelType, unsigned int mode, DriverChannelOptions options, DriverRef _Nonnull pDriver, IOChannelRef _Nullable * _Nonnull pOutSelf);

#define DriverChannel_GetOffset(__self) \
(__self)->offset

#endif /* DriverChannel_h */
