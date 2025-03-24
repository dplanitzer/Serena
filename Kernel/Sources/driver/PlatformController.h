//
//  PlatformController.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/17/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PlatformController_h
#define PlatformController_h

#include <driver/Driver.h>


// A platform controller is the root driver of a platform. All other drivers are
// direct or indirect children of the platform controller. It represents the
// motherboard hardware in that sense and it kicks off the detection of hardware
// that is part of the motherboard.
// A platform controller is expected to implement the synchronous driver model.
open_class(PlatformController, Driver,
    DriverCatalogId baseBusId;
);
open_class_funcs(PlatformController, Driver,
);


// Creates a platform controller instance. 'baseBusId' is the catalog id of the
// bus directory into which the platform controller should publish its immediate
// children. 
extern errno_t PlatformController_Create(Class* _Nonnull pClass, DriverCatalogId baseBusId, DriverRef _Nullable * _Nonnull pOutSelf);

#endif /* PlatformController_h */
