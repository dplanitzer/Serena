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
#include <diskcache/DiskCache.h>


// FSContainer which represents a single disk or disk partition.
open_class(DiskFSContainer, FSContainer,
    DiskCacheRef _Nonnull   diskCache;
    DiskSession             session;
);
open_class_funcs(DiskFSContainer, FSContainer,
);


extern errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskFSContainer_h */
