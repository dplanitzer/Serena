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
    InodeRef _Nonnull       driverNode;
    DiskCacheRef _Nonnull   diskCache;
    DiskSession             session;
);
open_class_funcs(DiskContainer, FSContainer,
);


extern errno_t DiskContainer_Create(InodeRef _Locked _Nonnull driverNode, unsigned int mode, FSContainerRef _Nullable * _Nonnull pOutSelf);

extern InodeRef _Nonnull DiskContainer_GetDriverNode(DiskContainerRef _Nonnull self);

#endif /* DiskContainer_h */
