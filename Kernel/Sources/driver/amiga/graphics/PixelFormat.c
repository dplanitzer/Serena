//
//  PixelFormat.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/24/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#include "PixelFormat.h"


// Returns how many planes are needed to store a pixel in the given pixel format.
// Returns 1 if the pixel format is a direct pixel format.
size_t PixelFormat_GetPlaneCount(PixelFormat format)
{
    switch (format) {
        case kPixelFormat_RGB_Indexed1:
        case kPixelFormat_RGB_Indexed2:
        case kPixelFormat_RGB_Indexed3:
        case kPixelFormat_RGB_Indexed4:
        case kPixelFormat_RGB_Indexed5:
            return format + 1;

        default:
            return 1;
    }
}

// Returns the number of entries the hardware CLUT supports if the screen is
// configured for the given pixel format. Returns 0 if the pixel format is not
// a CLUT-based format. 
size_t PixelFormat_GetCLUTEntryCount(PixelFormat format)
{
    switch (format) {
        case kPixelFormat_RGB_Indexed1:
        case kPixelFormat_RGB_Indexed2:
        case kPixelFormat_RGB_Indexed3:
        case kPixelFormat_RGB_Indexed4:
        case kPixelFormat_RGB_Indexed5:
            return 2 << (format << 1);

        default:
            return 0;
    }
}
