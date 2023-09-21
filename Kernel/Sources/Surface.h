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


// Specifies what you want to do with the pixels when you call LockPixels()
typedef enum _SurfaceAccess {
    kSurfaceAccess_Read,
    kSurfaceAccess_Write
} SurfaceAccess;


#define MAX_PLANE_COUNT  6
#define SURFACE_FLAG_LOCKED 0x01

typedef struct _Surface {
    Byte* _Nullable planes[MAX_PLANE_COUNT];
    Int16           width;
    Int16           height;
    Int16           bytesPerRow;
    Int16           planeCount;
    Int16           pixelFormat;
    UInt16          flags;
} Surface;


extern ErrorCode Surface_Create(Int width, Int height, PixelFormat pixelFormat, Surface* _Nullable * _Nonnull pOutSurface);
extern void Surface_Destroy(Surface* _Nullable pSurface);

extern Size Surface_GetPixelSize(Surface* _Nonnull pSurface);

extern ErrorCode Surface_LockPixels(Surface* _Nonnull pSurface, SurfaceAccess access);
extern void Surface_UnlockPixels(Surface* _Nonnull pSurface);

#endif /* Framebuffer_h */
