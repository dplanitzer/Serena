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


// A RamDisk object manages a virtual disk that stores the disk blocks in RAM.
// Disk blocks are allocated on demand. Blocks are internally organized and
// allocated in the form of 'extents'. The RAM disk user can specify the size
// of a block, an extent and the disk.
final_class(RamDisk, DiskDriver);


// Creates a new ROM disk instance. The disk data is provided by the contiguous
// memory block 'pDiskImage' which contains 'nBlockCount' disk blocks of size
// 'nBlockSize'. 'nBlockSize' must be a power-of-2. The disk instance takes
// ownership of the provided disk image if 'freeOnClose' is true. This means
// that the RomDisk object will call free() on the provided 'pDiskImage' when
// the RomDisk instance is deallocated. The RomDisk instance will do nothing
// with 'pDiskImage' if 'freeOnClose' is false and the RomDisk object is
// deallocated.
// Note that the provided disk image is expected to be initialized with a valid
// file system since there is no way to write to this disk.
extern errno_t RamDisk_Create(const char* _Nonnull name, size_t blockSize, LogicalBlockCount blockCount, LogicalBlockCount extentBlockCount, RamDiskRef _Nullable * _Nonnull pOutSelf);

#endif /* RamDisk_h */
