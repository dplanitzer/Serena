//
//  Surface.h
//  Apollo
//
//  Created by Dietmar Planitzer on 2/7/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Surface_h
#define Surface_h

#include <klib/klib.h>


// The pixel formats supported by framebuffers
typedef enum _PixelFormat {
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


// Specifies what you want to do with the pixels when you call LockPixels()
typedef enum _SurfaceAccess {
    kSurfaceAccess_Read,
    kSurfaceAccess_Write
} SurfaceAccess;


#define MAX_PLANE_COUNT  6
#define SURFACE_FLAG_LOCKED 0x01

typedef struct _Surface {
    uint8_t* _Nullable  planes[MAX_PLANE_COUNT];
    int16_t             width;
    int16_t             height;
    int16_t             bytesPerRow;
    int16_t             planeCount;
    int16_t             pixelFormat;
    uint16_t            flags;
} Surface;


extern errno_t Surface_Create(int width, int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern void Surface_Destroy(Surface* _Nullable pSurface);

extern Size Surface_GetPixelSize(Surface* _Nonnull pSurface);

extern errno_t Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access);
extern void Surface_UnlockPixels(Surface* _Nonnull pSurface);

#endif /* Surface_h */
