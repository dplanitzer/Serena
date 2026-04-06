//
//  video_conf.c
//  kernel
//
//  Created by Dietmar Planitzer on 9/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#include "video_conf.h"
#include <hal/hw/m68k-amiga/chipset.h>
#include <kern/kernlib.h>


#define NUM_CONFS  8
static const video_conf_t g_video_conf[NUM_CONFS] = {
// [0]  NTSC 320x200 60fps
{320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    5, true, true, 6
},
// [1]  NTSC 640x200 60fps
{640, 200, 60,
    VCFLAG_HIRES,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 0,
    4, false, false, 4
},
// [2]  NTSC 320x400 30fps (interlaced)
{320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    5, true, true, 6
},
// [3]  NTSC 640x400 30fps (interlaced)
{640, 400, 30,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 1,
    4, false, false, 4
},
// [4]  PAL 320x256 50fps
{320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    5, true, true, 6
},
// [5]  PAL 640x256 50fps
{640, 256, 50,
    VCFLAG_HIRES,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 0,
    4, false, false, 4
},
// [6]  PAL 320x512 25fps (interlaced)
{320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    5, true, true, 6
},
// [7]  PAL 640x512 25fps (interlaced)
{640, 512, 25,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 1,
    4, false, false, 4
},
};


const video_conf_t* _Nonnull get_null_video_conf(void)
{
    return (chipset_is_ntsc()) ? &g_video_conf[0] : &g_video_conf[4];
}

static bool _matches_pixel_format(const video_conf_t* _Nonnull cfg, pixfmt_t fmt)
{
    switch (fmt) {
        case PIXFMT_RGB_IND_1:
        case PIXFMT_RGB_IND_2:
        case PIXFMT_RGB_IND_3:
        case PIXFMT_RGB_IND_4:
        case PIXFMT_RGB_IND_5:
        case PIXFMT_RGB_IND_6:
        case PIXFMT_RGB_IND_7:
        case PIXFMT_RGB_IND_8:
            if (PixelFormat_GetPlaneCount(fmt) <= cfg->maxPlaneCount) {
                return true;
            }
            break;

        case PIXFMT_RGB_HAM_5:
        case PIXFMT_RGB_HAM_6:
            if (cfg->allowsHAM) {
                return true;
            }
            break;

        case PIXFMT_RGB_EHB_6:
            if (cfg->allowsEHB) {
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

const video_conf_t* _Nullable get_matching_video_conf(int width, int height, pixfmt_t fmt)
{
    for (size_t i = 0; i < NUM_CONFS; i++) {
        const video_conf_t* hwc = &g_video_conf[i];

        if (hwc->width == width
            && hwc->height == height
            && _matches_pixel_format(hwc, fmt)) {
            return hwc;
        }
    }

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: pixfmt_t
//

int8_t PixelFormat_GetPlaneCount(pixfmt_t format)
{
    switch (format) {
        case PIXFMT_RGB_SPRITE_2:
        case PIXFMT_RGB_IND_1:
            return 1;

        case PIXFMT_RGB_IND_2:
            return 2;

        case PIXFMT_RGB_IND_3:
            return 3;

        case PIXFMT_RGB_IND_4:
            return 4;

        case PIXFMT_RGB_HAM_5:
        case PIXFMT_RGB_IND_5:
            return 5;

        case PIXFMT_RGB_HAM_6:
        case PIXFMT_RGB_IND_6:
            return 6;

        case PIXFMT_RGB_IND_7:
            return 7;

        case PIXFMT_RGB_IND_8:
            return 8;
            
        default:
            return 1;
    }
}
