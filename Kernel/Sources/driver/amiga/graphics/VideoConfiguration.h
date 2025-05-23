//
//  VideoConfiguration.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/23/23.
//  Copyright © 2023 Dietmar Planitzer. All rights reserved.
//

#ifndef VideoConfiguration_h
#define VideoConfiguration_h

#include <kern/errno.h>
#include <kern/kernlib.h>
#include <kpi/fb.h>


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
#define MAX_LORES_WIDTH     368


typedef struct _VideoConfigurationRange {
    int16_t     width;
    int16_t     height;
    int8_t      fps;
    int8_t      pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
} _VideoConfigurationRange;



#define VideoConfiguration_GetPixelWidth(__self) \
((__self)->width)

#define VideoConfiguration_GetPixelHeight(__self) \
((__self)->height)

#define VideoConfiguration_GetRefreshRate(__self) \
((__self)->fps)

#define VideoConfiguration_IsInterlaced(__self) \
(((__self)->height > MAX_PAL_HEIGHT) ? true : false)

#define VideoConfiguration_IsHires(__self) \
(((__self)->width > MAX_LORES_WIDTH) ? true : false)

#define VideoConfiguration_IsPAL(__self) \
(((__self)->fps == 25 || (__self)->fps == 50) ? true : false)

#define VideoConfiguration_IsNTSC(__self) \
(((__self)->fps == 30 || (__self)->fps == 60) ? true : false)


extern errno_t VideoConfiguration_Validate(const VideoConfiguration* _Nonnull vidCfg, PixelFormat pixelFormat);
extern errno_t VideoConfiguration_GetNext(VideoConfigurationRange* _Nonnull config, size_t bufSize, size_t* _Nonnull pIter);

#endif /* VideoConfiguration_h */
