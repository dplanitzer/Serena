//
//  FloppyDisk.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDisk_h
#define FloppyDisk_h

#include <klib/klib.h>
#include <driver/DiskDriver.h>

#define MAX_FLOPPY_DISK_DRIVES  4

// Stores the state of a single floppy drive.
// Note that:
// - We detect at boot time which drives are actually connected and we create a
//   floppy disk driver instance for each detected drive.
// - disk changes are dynamically detected and handled. We detect a disk change
//   when we detect the drive and when we do I/O operations on the drive
// - loss of disk drive hardware is dynamically detected when we do I/O operations.
//   However, once a drive loss is detected teh driver stays in drive lost mode.
//   It does not attempt to redetect the drive hardware.

final_class(FloppyDisk, DiskDriver);


extern errno_t FloppyDisk_DiscoverDrives(FloppyDiskRef _Nullable pOutDrives[MAX_FLOPPY_DISK_DRIVES]);

extern bool FloppyDisk_HasDisk(FloppyDiskRef _Nonnull self);

#endif /* FloppyDisk_h */
