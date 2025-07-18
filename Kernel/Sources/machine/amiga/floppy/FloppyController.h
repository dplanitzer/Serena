//
//  FloppyController.h
//  kernel
//
//  Created by Dietmar Planitzer on 2/12/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef FloppyController_h
#define FloppyController_h

#include <driver/Driver.h>
#include <machine/amiga/floppy/FloppyDriver.h>


// The floppy controller. The Amiga has just one single floppy DMA channel
// which is shared by all drives.
final_class(FloppyController, Driver);


// Creates a floppy controller
extern errno_t FloppyController_Create(DriverRef _Nullable parent, FloppyControllerRef _Nullable * _Nonnull pOutSelf);

#endif /* FloppyController_h */
