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
typedef struct ScreenConfiguration {
    int16_t       uniqueId;
    int16_t       width;
    int16_t       height;
    int8_t        fps;
    uint8_t       diw_start_h;        // display window start
    uint8_t       diw_start_v;
    uint8_t       diw_stop_h;         // display window stop
    uint8_t       diw_stop_v;
    uint16_t      bplcon0;            // BPLCON0 template value
    uint8_t       spr_shift;          // Shift factors that should be applied to X & Y coordinates to convert them from screen coords to sprite coords [h:4,v:4]
    int8_t        pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
} ScreenConfiguration;


// The supported screen configurations
extern const ScreenConfiguration kScreenConfig_NTSC_320_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_640_200_60;
extern const ScreenConfiguration kScreenConfig_NTSC_320_400_30;
extern const ScreenConfiguration kScreenConfig_NTSC_640_400_30;

extern const ScreenConfiguration kScreenConfig_PAL_320_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_640_256_50;
extern const ScreenConfiguration kScreenConfig_PAL_320_512_25;
extern const ScreenConfiguration kScreenConfig_PAL_640_512_25;

extern int ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* _Nonnull self);
extern int ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* _Nonnull self);
extern int ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* _Nonnull self);
extern bool ScreenConfiguration_IsInterlaced(const ScreenConfiguration* _Nonnull self);
extern bool ScreenConfiguration_IsHires(const ScreenConfiguration* _Nonnull self);

#endif /* ScreenConfiguration_h */
