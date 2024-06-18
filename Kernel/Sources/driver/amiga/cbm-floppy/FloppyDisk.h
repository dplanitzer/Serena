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
final_class(FloppyDisk, DiskDriver);


extern errno_t FloppyDisk_DiscoverDrives(FloppyDiskRef _Nullable pOutDrives[MAX_FLOPPY_DISK_DRIVES]);

#endif /* FloppyDisk_h */
