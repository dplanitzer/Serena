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
#include <machine/amiga/chipset.h>


#define MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION   5


typedef struct _VideoConfigurationRange {
    int16_t     width;
    int16_t     height;
    int8_t      fps;
    int8_t      pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS_PER_VIDEO_CONFIGURATION];
} _VideoConfigurationRange;

typedef struct VideoConfiguration {
    int     width;
    int     height;
    int     fps;
} VideoConfiguration;


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


// Returns the numbers of colors available for a pixel for the given pixel format.
// 0 is returned if the pixel format isn't supported.
extern size_t PixelFormat_GetColorDepth(PixelFormat pm);

#endif /* VideoConfiguration_h */
