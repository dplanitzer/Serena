//
//  gd/device.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_DEVICE_H
#define _GD_DEVICE_H 1

#include <gd/types.h>

extern gd_device_t _Nullable gdCreateMainDevice(void);
extern gd_device_t _Nullable gdCreateDevice(const char* _Nonnull device_path);
extern void gdDestroyDevice(gd_device_t _Nullable device);

#endif /* _GD_DEVICE_H */
