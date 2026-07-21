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


const video_conf_t g_video_conf[NUM_VIDEO_CONFIGS] = {
// [0]  NTSC 320x200 60fps
{GD_COLOR_INDEX3, 320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    6
},
{GD_COLOR_INDEX4, 320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    6
},
{GD_COLOR_INDEX5, 320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    6
},
{GD_RGB_EHB_6, 320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    6
},
{GD_RGB_HAM_6, 320, 200, 60,
    0,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 0,
    6
},

// [5]  NTSC 640x200 60fps
{GD_COLOR_INDEX3, 640, 200, 60,
    VCFLAG_HIRES,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 0,
    4
},
{GD_COLOR_INDEX4, 640, 200, 60,
    VCFLAG_HIRES,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 0,
    4
},

// [7]  NTSC 320x400 30fps (interlaced)
{GD_COLOR_INDEX3, 320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    6
},
{GD_COLOR_INDEX4, 320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    6
},
{GD_COLOR_INDEX5, 320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    6
},
{GD_RGB_EHB_6, 320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    6
},
{GD_RGB_HAM_6, 320, 400, 30,
    VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 0, 1,
    6
},

// [12]  NTSC 640x400 30fps (interlaced)
{GD_COLOR_INDEX3, 640, 400, 30,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 1,
    4
},
{GD_COLOR_INDEX4, 640, 400, 30,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_NTSC_HSTART, DIW_NTSC_HSTOP, DIW_NTSC_VSTART, DIW_NTSC_VSTOP,
    DIW_NTSC_HSTART, DIW_NTSC_VSTART, 1, 1,
    4
},

// [14]  PAL 320x256 50fps
{GD_COLOR_INDEX3, 320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    6
},
{GD_COLOR_INDEX4, 320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    6
},
{GD_COLOR_INDEX5, 320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    6
},
{GD_RGB_EHB_6, 320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    6
},
{GD_RGB_HAM_6, 320, 256, 50,
    0,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 0,
    6
},

// [19]  PAL 640x256 50fps
{GD_COLOR_INDEX3, 640, 256, 50,
    VCFLAG_HIRES,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 0,
    4
},
{GD_COLOR_INDEX4, 640, 256, 50,
    VCFLAG_HIRES,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 0,
    4
},

// [21]  PAL 320x512 25fps (interlaced)
{GD_COLOR_INDEX3, 320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    6
},
{GD_COLOR_INDEX4, 320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    6
},
{GD_COLOR_INDEX5, 320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    6
},
{GD_RGB_EHB_6, 320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    6
},
{GD_RGB_HAM_6, 320, 512, 25,
    VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 0, 1,
    6
},

// [26]  PAL 640x512 25fps (interlaced)
{GD_COLOR_INDEX3, 640, 512, 25,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 1,
    4
},
{GD_COLOR_INDEX4, 640, 512, 25,
    VCFLAG_HIRES | VCFLAG_LACE,
    DIW_PAL_HSTART, DIW_PAL_HSTOP, DIW_PAL_VSTART, DIW_PAL_VSTOP,
    DIW_PAL_HSTART, DIW_PAL_VSTART, 1, 1,
    4
},
};


const video_conf_t* _Nonnull get_null_video_conf(void)
{
    return (chipset_is_ntsc()) ? &g_video_conf[0] : &g_video_conf[14];
}

const video_conf_t* _Nullable get_matching_video_conf(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nonnull params)
{
    for (size_t i = 0; i < NUM_VIDEO_CONFIGS; i++) {
        const video_conf_t* hwc = &g_video_conf[i];

        if (hwc->width == mode->width
            && hwc->height == mode->height
            && hwc->refreshRate == mode->refreshRate
            && hwc->pixelFormat == mode->pixelFormat) {
            return hwc;
        }
    }

    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: gd_pixfmt_t
//

int8_t PixelFormat_GetPlaneCount(gd_pixfmt_t format)
{
    switch (format) {
        case GD_RGB_SPRITE_2:
        case GD_COLOR_INDEX1:
            return 1;

        case GD_COLOR_INDEX2:
            return 2;

        case GD_COLOR_INDEX3:
            return 3;

        case GD_COLOR_INDEX4:
            return 4;

        case GD_COLOR_INDEX5:
            return 5;

        case GD_RGB_HAM_6:
        case GD_COLOR_INDEX6:
            return 6;

        case GD_COLOR_INDEX7:
            return 7;

        case GD_COLOR_INDEX8:
            return 8;
            
        default:
            return 1;
    }
}
