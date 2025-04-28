//
//  PartitionDisk.h
//  kernel
//
//  Created by Dietmar Planitzer on 12/26/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PartitionDisk_h
#define PartitionDisk_h

#include "DiskDriver.h"


// A partition disk driver manages a partition of a (fixed) disk. It delegates
// the actual I/O operations to the underlying (whole) disk driver. 
final_class(PartitionDriver, DiskDriver);


extern errno_t PartitionDriver_Create(DriverRef _Nullable parent, const char* _Nonnull name, sno_t lsaStart, scnt_t sectorCount, bool isReadOnly, DiskDriverRef wholeDisk, PartitionDriverRef _Nullable * _Nonnull pOutSelf);

#endif /* PartitionDisk_h */
