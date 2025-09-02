//
//  video_conf.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _VIDEO_CONF_H
#define _VIDEO_CONF_H

#include <kern/types.h>
#include <kpi/fb.h>


#define VCFLAG_HIRES   1
#define VCFLAG_LACE    2

#define MAX_PIXEL_FORMATS   5

typedef struct video_conf {
    int16_t     width;
    int16_t     height;
    int8_t      fps;
    uint8_t     flags;
    uint8_t     hDwStart;
    uint8_t     hDwStop;
    uint8_t     vDwStart;
    uint8_t     vDwStop;
    uint8_t     hSprOrigin;
    uint8_t     vSprOrigin;
    uint8_t     hSprScale;
    uint8_t     vSprScale;
    int8_t      pixelFormatCount;   // Number of supported pixel formats
    PixelFormat pixelFormat[MAX_PIXEL_FORMATS];
} video_conf_t;


// Returns the video conf that should be used for a null Copper program.
extern const video_conf_t* _Nonnull get_null_video_conf(void);

// Looks up the video configuration that corresponds to the given screen
// configuration.
extern const video_conf_t* _Nullable get_matching_video_conf(int width, int height, PixelFormat fmt);

#endif /* _VIDEO_CONF_H */
