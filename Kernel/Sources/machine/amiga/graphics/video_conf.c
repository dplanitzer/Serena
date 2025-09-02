//
//  video_conf.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "video_conf.h"
#include <kern/kernlib.h>
#include <machine/amiga/chipset.h>


#define NUM_CONFS  8
static const video_conf_t g_video_conf[NUM_CONFS] = {
// NTSC 320x200 60fps
{320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x200 60fps
{640, 200, 60,
    VCFLAG_HIRES,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 0,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// NTSC 320x400 30fps (interlaced)
{320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// NTSC 640x400 30fps (interlaced)
{640, 400, 30,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 1,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x256 50fps
{320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x256 50fps
{640, 256, 50,
    VCFLAG_HIRES,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 0,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
// PAL 320x512 25fps (interlaced)
{320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}
},
// PAL 640x512 25fps (interlaced)
{640, 512, 25,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 1,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}
},
};


const video_conf_t* _Nullable get_matching_video_conf(int width, int height, PixelFormat fmt)
{
    for (size_t i = 0; i < NUM_CONFS; i++) {
        const video_conf_t* hwc = &g_video_conf[i];

        if (hwc->width == width
            && hwc->height == height) {
            for (int8_t i = 0; i < MAX_PIXEL_FORMATS; i++) {
                if (hwc->pixelFormat[i] == fmt) {
                    return hwc;
                }
            }
        }
    }

    return NULL;
}
