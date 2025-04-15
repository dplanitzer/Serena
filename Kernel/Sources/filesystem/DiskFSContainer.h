//
//  DiskFSContainer.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskFSContainer_h
#define DiskFSContainer_h

#include <filesystem/FSContainer.h>


// FSContainer which represents a single disk or disk partition.
open_class(DiskFSContainer, FSContainer,
    IOChannelRef _Nonnull           channel;
    DiskDriverRef _Nonnull _Weak    disk;
    DiskCacheRef _Nonnull           diskCache;
);
open_class_funcs(DiskFSContainer, FSContainer,
);


extern errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskFSContainer_h */
