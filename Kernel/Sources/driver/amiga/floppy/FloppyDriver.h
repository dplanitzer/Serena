//
//  FloppyDriver.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyDriver_h
#define FloppyDriver_h

#include <driver/disk/DiskDriver.h>

// Stores the state of a single floppy drive.
// Note that:
// - We detect at boot time which drives are actually connected and we create a
//   floppy disk driver instance for each detected drive.
// - disk changes are dynamically detected and handled. We detect a disk change
//   when we detect the drive and when we do I/O operations on the drive
// - loss of disk drive hardware is dynamically detected when we do I/O operations.
//   However, once a drive loss is detected teh driver stays in drive lost mode.
//   It does not attempt to re-detect the drive hardware.
extern const char* const kFloppyDrive0Name;


final_class(FloppyDriver, DiskDriver);

extern bool FloppyDriver_HasDisk(FloppyDriverRef _Nonnull self);

#endif /* FloppyDriver_h */
