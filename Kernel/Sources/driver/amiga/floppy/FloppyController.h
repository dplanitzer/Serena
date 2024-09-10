//
//  FloppyController.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyController_h
#define FloppyController_h

#include <klib/klib.h>
#include <kobj/Object.h>

//class_forward(FloppyDisk);
struct FloppyDisk;

#define MAX_FLOPPY_DISK_DRIVES  4

// The floppy controller. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
final_class(FloppyController, Object);


// Creates the floppy controller
extern errno_t FloppyController_Create(FloppyControllerRef _Nullable * _Nonnull pOutSelf);

extern errno_t FloppyController_DiscoverDrives(FloppyControllerRef _Nonnull self, struct FloppyDisk* _Nullable pOutDrives[MAX_FLOPPY_DISK_DRIVES]);

#endif /* FloppyController_h */
