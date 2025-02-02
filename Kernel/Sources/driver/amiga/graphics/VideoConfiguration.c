//
//  VideoConfiguration.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "VideoConfiguration.h"


// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
static const VideoConfigurationRange gSupportedRanges[] = {
// NTSC 320x200 60fps
{320, 200, 60,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x200 60fps
{640, 200, 60,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// NTSC 320x400 30fps (interlaced)
{320, 400, 30,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x400 30fps (interlaced)
{640, 400, 30,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x256 50fps
{320, 256, 50,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x256 50fps
{640, 256, 50,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x512 25fps (interlaced)
{320, 512, 25,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x512 25fps (interlaced)
{640, 512, 25,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// NULL
{0, 0, 0, 0},
};


errno_t VideoConfiguration_Validate(const VideoConfiguration* _Nonnull vidCfg, PixelFormat pixelFormat)
{
    const VideoConfigurationRange* vcr = gSupportedRanges;

    while (vcr->width > 0) {
        if (vcr->width == vidCfg->width
            && vcr->height == vidCfg->height
            && vcr->fps == vidCfg->fps) {
            for (int i = 0; i < MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION; i++) {
                if (vcr->pixelFormat[i] == pixelFormat) {
                    return EOK;
                }
            }
        }

        vcr++;
    }

    return ENOTSUP;
}
