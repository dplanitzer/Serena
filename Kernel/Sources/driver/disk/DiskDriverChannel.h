//
//  DiskDriverChannel.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef DiskDriverChannel_h
#define DiskDriverChannel_h

#include <driver/DriverChannel.h>
#include <System/Disk.h>


// A disk driver channel stores a snapshot of the disk geometry and id and media
// id to ensure that the channel is tied to the particular disk that was in the
// driver when the channel was opened. If the media is replaced with another
// media while the channel is open, then all further read/write operations on
// the channel must fail.
open_class(DiskDriverChannel, DriverChannel,
    off_t       diskSize;
    DiskInfo    info;
);
open_class_funcs(DiskDriverChannel, DriverChannel,
);


extern errno_t DiskDriverChannel_Create(DiskDriverRef _Nonnull pDriver, const DiskInfo* _Nonnull info, unsigned int mode, IOChannelRef _Nullable * _Nonnull pOutSelf);

#define DiskDriverChannel_GetInfo(__self) \
((const DiskInfo*)&((DiskDriverChannelRef)(__self))->info)

#endif /* DiskDriverChannel_h */
