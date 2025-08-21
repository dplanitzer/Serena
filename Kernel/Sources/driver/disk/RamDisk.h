//
//  RamDisk.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef RamDisk_h
#define RamDisk_h

#include "DiskDriver.h"


// A RamDisk object manages a virtual disk that stores the disk sectors in RAM.
// Disk sectors are allocated on demand. Sectors are internally organized and
// allocated in the form of 'extents'. The RAM disk user can specify the size
// of a sector, an extent and the disk.
final_class(RamDisk, DiskDriver);


// Creates a new ROM disk instance. The disk data is provided by the contiguous
// memory block 'pDiskImage' which contains 'sectorCount' sectors of size
// 'sectorSize'. 'sectorSize' must be a power-of-2.
extern errno_t RamDisk_Create(const char* _Nonnull name, size_t sectorSize, scnt_t sectorCount, scnt_t extentSectorCount, RamDiskRef _Nullable * _Nonnull pOutSelf);

#endif /* RamDisk_h */
