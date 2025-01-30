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


#define MAX_CLUT_ENTRIES    32
#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5


// Amiga Hardware Reference, 3rd Edition, p59
#define DIW_NTSC_HSTART     0x81
#define DIW_NTSC_VSTART     0x2c
#define DIW_NTSC_HSTOP      0xc1
#define DIW_NTSC_VSTOP      0xf4

#define DIW_PAL_HSTART      0x81
#define DIW_PAL_VSTART      0x2c
#define DIW_PAL_HSTOP       0xc1
#define DIW_PAL_VSTOP       0x2c


// Amiga Hardware Reference, 3rd Edition, p79
#define MAX_NTSC_HEIGHT         241
#define MAX_NTSC_LACE_HEIGHT    483
#define MAX_PAL_HEIGHT          283
#define MAX_PAL_LACE_HEIGHT     567


// Amiga Hardware Reference, 3rd Edition, p80
#define MAX_LORES_WIDTH         368

#define FPS_PAL     25
#define FPS_NTSC    30


typedef struct ScreenConfiguration {
    int16_t       uniqueId;
    int16_t       width;
    int16_t       height;
    int8_t        fps;
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

extern size_t ScreenConfiguration_GetPixelWidth(const ScreenConfiguration* _Nonnull self);
extern size_t ScreenConfiguration_GetPixelHeight(const ScreenConfiguration* _Nonnull self);
extern size_t ScreenConfiguration_GetRefreshRate(const ScreenConfiguration* _Nonnull self);

#define ScreenConfiguration_IsInterlaced(__self) \
(((__self)->height > MAX_PAL_HEIGHT) ? true : false)

#define ScreenConfiguration_IsHires(__self) \
(((__self)->width > MAX_LORES_WIDTH) ? true : false)

#define ScreenConfiguration_IsPal(__self) \
(((__self)->fps == FPS_PAL) ? true : false)

#endif /* ScreenConfiguration_h */
