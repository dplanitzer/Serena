//
//  gd/display.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/13/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_DISPLAY_H
#define _GD_DISPLAY_H 1

#include <stdbool.h>
#include <gd/types.h>


//
// CLUT
//

extern void gdClut(gd_device_t _Nonnull device, size_t idx, size_t count, const gd_rgb32_t* _Nonnull entries);
extern void gdGetClut(gd_device_t _Nonnull device, size_t idx, size_t count, gd_rgb32_t* _Nonnull entries);
extern void gdGetClutInfo(gd_device_t _Nonnull device, gd_clut_info_t* _Nonnull info);


//
// Display
//

extern void gdDisplayMode(gd_device_t _Nonnull device, const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nullable params, int op);
extern void gdGetDisplayInfo(gd_device_t _Nonnull device, int flavor, gd_display_info_ref _Nonnull pOutInfo);
extern bool gdEnumDisplayModes(gd_device_t _Nonnull device, int index, gd_display_mode_t* _Nonnull pOutMode);

#endif /* _GD_DISPLAY_H */
