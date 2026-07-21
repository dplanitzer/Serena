//
//  video_conf.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/2/25.
//  Copyright © 2025 Dietmar Planitzer. All rights reserved.
//

#ifndef _VIDEO_CONF_H
#define _VIDEO_CONF_H

#include <stdbool.h>
#include <stdint.h>
#include <kpi/framebuffer.h>

#define VCFLAG_HIRES   1
#define VCFLAG_LACE    2

typedef struct video_conf {
    gd_pixfmt_t pixelFormat;
    int16_t     width;
    int16_t     height;
    int8_t      refreshRate;
    uint8_t     flags;
    uint8_t     hDwStart;
    uint8_t     hDwStop;
    uint8_t     vDwStart;
    uint8_t     vDwStop;
    uint8_t     hSprOrigin;
    uint8_t     vSprOrigin;
    uint8_t     hSprScale;
    uint8_t     vSprScale;
    int8_t      maxPlaneCountDPF;   // Dual playfield display mode supported, if > 0. This is the max plane count across both playfields
} video_conf_t;


#define NUM_VIDEO_CONFIGS   28
extern const video_conf_t g_video_conf[NUM_VIDEO_CONFIGS];


// Returns the video conf that should be used for a null Copper program.
extern const video_conf_t* _Nonnull get_null_video_conf(void);

// Looks up the video configuration that corresponds to the given screen
// configuration.
extern const video_conf_t* _Nullable get_matching_video_conf(const gd_display_mode_t* _Nonnull mode, const gd_display_params_t* _Nonnull params);


// Returns how many planes are needed to store a pixel in the given pixel format.
// Returns 1 if the pixel format is a direct pixel format.
extern int8_t PixelFormat_GetPlaneCount(gd_pixfmt_t format);

#endif /* _VIDEO_CONF_H */
