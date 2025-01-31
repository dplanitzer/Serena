//
//  ScreenConfiguration.c
//  kernel
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "ScreenConfiguration.h"
#include <hal/Platform.h>


// DDIWSTART = specific to mode. See hardware reference manual
// DDIWSTOP = last 8 bits of pixel position
const ScreenConfiguration kScreenConfig_NTSC_320_200_60 = {0, 320, 200, 60,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_NTSC_640_200_60 = {1, 640, 200, 60,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const ScreenConfiguration kScreenConfig_NTSC_320_400_30 = {2, 320, 400, 30,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_NTSC_640_400_30 = {3, 640, 400, 30,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};

const ScreenConfiguration kScreenConfig_PAL_320_256_50 = {4, 320, 256, 50,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_PAL_640_256_50 = {5, 640, 256, 50,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
const ScreenConfiguration kScreenConfig_PAL_320_512_25 = {6, 320, 512, 25,
    5, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4,
        kPixelFormat_RGB_Indexed5}};
const ScreenConfiguration kScreenConfig_PAL_640_512_25 = {7, 640, 512, 25,
    4, {kPixelFormat_RGB_Indexed1,
        kPixelFormat_RGB_Indexed2,
        kPixelFormat_RGB_Indexed3,
        kPixelFormat_RGB_Indexed4}};
