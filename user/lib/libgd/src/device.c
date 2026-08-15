//
//  device.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <gd/device.h>
#include <stdlib.h>
#include <serena/file.h>


gd_device_t _Nullable gdCreateMainDevice(void)
{
    return gdCreateDevice("/dev/gd0");
}

gd_device_t _Nullable gdCreateDevice(const char* _Nonnull device_path)
{
    struct gd_device* dp = calloc(1, sizeof(struct gd_device));

    if (dp == NULL) {
        return NULL;
    }

    dp->fd = fs_open(NULL, device_path, O_RDWR);
    if (dp->fd < 0) {
        free(dp);
        return NULL;
    }

    return dp;
}

void gdDestroyDevice(gd_device_t _Nullable device)
{
    struct gd_device* dp = device;

    if (dp) {
        fd_close(dp->fd);
        free(dp);
    }
}
