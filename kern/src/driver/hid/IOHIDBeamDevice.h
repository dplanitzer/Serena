//
//  IOHIDBeamDevice.h
//  kernel
//
//  Created by Dietmar Planitzer on 7/8/26.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef IOHIDBeamDevice_h
#define IOHIDBeamDevice_h

#include <driver/IODriver.h>
#include <kpi/framebuffer.h>


// A HID beam device provides an abstraction over video hardware that allows a
// light pen driver to get the current position of the video beam.
//
// A beam device is an exclusive device.
//
open_class(IOHIDBeamDevice, IODriver,
);
open_class_funcs(IOHIDBeamDevice, IODriver,

    // Returns the current position of the video beam and true if the beam was
    // triggered by a light pen; false otherwise.
    // Override: Required
    // Default: Does nothing
    bool (*getBeamPosition)(void* _Nonnull self, int16_t* _Nonnull x, int16_t* _Nonnull y);
);


//
// Subclassers
//

#define IOHIDBeamDevice_GetBeamPosition(__self, __x, __y) \
invoke_n(getBeamPosition, IOHIDBeamDevice, __self, __x, __y)

#endif /* IOHIDBeamDevice_h */
