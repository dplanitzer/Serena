//
//  DiskContainer.h
//  kernel
//
//  Created by Dietmar Planitzer on 10/12/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskContainer_h
#define DiskContainer_h

#include <filesystem/FSContainer.h>
#include <diskcache/DiskCache.h>


// FSContainer which represents a single disk or disk partition.
open_class(DiskContainer, FSContainer,
    DiskCacheRef _Nonnull   diskCache;
    DiskSession             session;
);
open_class_funcs(DiskContainer, FSContainer,
);


extern errno_t DiskContainer_Create(IOChannelRef _Nonnull pChannel, FSContainerRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskContainer_h */
