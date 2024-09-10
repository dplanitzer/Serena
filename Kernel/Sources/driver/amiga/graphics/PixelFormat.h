//
//  PixelFormat.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/24/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef PixelFormat_h
#define PixelFormat_h

#include <klib/Types.h>


// The pixel formats supported by framebuffers
typedef enum PixelFormat {
    kPixelFormat_RGB_Indexed1,      // planar indexed RGB with 1 plane
    kPixelFormat_RGB_Indexed2,      // planar indexed RGB with 2 planes
    kPixelFormat_RGB_Indexed3,      // planar indexed RGB with 3 planes
    kPixelFormat_RGB_Indexed4,      // planar indexed RGB with 4 planes
    kPixelFormat_RGB_Indexed5,      // planar indexed RGB with 5 planes
} PixelFormat;


// Returns how many planes are needed to store a pixel in the given pixel format.
// Returns 1 if the pixel format is a direct pixel format.
extern size_t PixelFormat_GetPlaneCount(PixelFormat format);

// Returns the number of entries the hardware CLUT supports if the screen is
// configured for the given pixel format. Returns -1 if the pixel format is not
// a CLUT-based format. 
extern size_t PixelFormat_GetCLUTCapacity(PixelFormat format);

#endif /* PixelFormat_h */
