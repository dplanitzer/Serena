//
//  display.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include <gd/display.h>
#include <kpi/gd_core.h>
#include <errno.h>
#include "gd_priv.h"


void gdClut(gd_device_t _Nonnull device, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_CLUT, idx, count, entries);
}

void gdGetClut(gd_device_t _Nonnull device, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_GET_CLUT, idx, count, entries);
}

void gdGetClutInfo(gd_device_t _Nonnull device, gd_clut_info_t* _Nonnull info)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_GET_CLUT_INFO, info);
}


void gdDisplayMode(gd_device_t _Nonnull device, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_DISPLAY_MODE, mode, params, op);
}

void gdGetDisplayInfo(gd_device_t _Nonnull device, int flavor, gd_display_info_ref _Nonnull pOutInfo)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_GET_DISPLAY_INFO, flavor, pOutInfo);
}

bool gdEnumDisplayModes(gd_device_t _Nonnull device, int index, gd_display_mode_t* _Nonnull pOutMode)
{
    struct gd_device* dp = device;

    if (fd_cntl(dp->fd, GDC_ENUM_DISPLAY_MODES, index, pOutMode) == 0) {
        return true;
    }
    else {
        errno = 0;
        return false;
    }
}
