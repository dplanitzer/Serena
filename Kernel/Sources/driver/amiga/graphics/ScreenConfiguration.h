//
//  ScreenConfiguration.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef ScreenConfiguration_h
#define ScreenConfiguration_h

#include <klib/Types.h>
#include "PixelFormat.h"


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5
struct ScreenConfiguration {
    int16_t       uniqueId;
    int16_t       width;
    int16_t       height;
    int8_t        fps;
    uint8_t       diw_start_h;        // display window start
    uint8_t       diw_start_v;
    uint8_t       diw_stop_h;         // display window stop
    uint8_t       diw_stop_v;
    uint8_t       ddf_start;          // data fetch start
    uint8_t       ddf_stop;           // data fetch stop
    uint8_t       ddf_mod;            // number of padding bytes stored in memory between scan lines
    uint16_t      bplcon0;            // BPLCON0 template value
    uint8_t       spr_shift;          // Shift factors that should be applied to X & Y coordinates to convert them from screen coords to sprite coords [h:4,v:4]
    int8_t        pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
};

#ifndef __screen_configuration_is_defined
#define __screen_configuration_is_defined 1
typedef struct ScreenConfiguration ScreenConfiguration;
#endif

#endif /* ScreenConfiguration_h */
