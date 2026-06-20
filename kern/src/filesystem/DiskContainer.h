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
final_class(DiskContainer, FSContainer);


extern errno_t DiskContainer_Create(DiskDriverRef _Nonnull disk, unsigned int mode, FSContainerRef _Nullable * _Nonnull pOutSelf);

#endif /* DiskContainer_h */
