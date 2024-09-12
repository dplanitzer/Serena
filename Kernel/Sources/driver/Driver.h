//
//  Driver.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/11/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef Driver_h
#define Driver_h

#include <kobj/Object.h>


// A driver object binds to and manages a device. A device is a piece of hardware
// that does some work. In the device tree, all driver objects are leaf nodes in
// the tree.
open_class_with_ref(Driver, Object,
);
open_class_funcs(Driver, Object,
);


// A driver controller is a driver that binds to some kind of bus and manages this
// bus and the driver objects managing hardware that is connected to the bus. In
// the device tree, all inner nodes of the tree are represented by driver
// controllers.
open_class_with_ref(DriverController, Driver,
);
open_class_funcs(DriverController, Driver,
);

#endif /* Driver_h */
