//
//  RomDisk.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/29/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef RomDisk_h
#define RomDisk_h

#include "DiskDriver.h"


// A RomDisk object manages a virtual disk that stores the disk blocks in
// read-only memory like a physical ROM or EPROM. Note that the disk expects
// that you provide a memory region that holds the pre-initialized disk blocks
// when you create it. You can instruct the ROM disk to take ownership of this
// memory region which means that the RomDisk will call free() on the provided
// memory pointer when the RomDisk is deallocated.
final_class(RomDisk, DiskDriver);


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
extern errno_t RomDisk_Create(const void* _Nonnull pDiskImage, size_t nBlockSize, LogicalBlockCount nBlockCount, bool freeOnClose, RomDiskRef _Nullable * _Nonnull pOutSelf);

#endif /* RomDisk_h */
