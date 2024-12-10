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
final_class(DiskFSContainer, FSContainer);


extern errno_t DiskFSContainer_Create(IOChannelRef _Nonnull pChannel, DriverId diskId, MediaId mediaId, FSContainerRef _Nullable * _Nonnull pOutContainer);

#endif /* DiskFSContainer_h */
